/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
                      Digital Media (EDM) (http://www.edm.uhasselt.be)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  
  USA

*/

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_OSS

#include "mipossinputoutput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include "mipcompat.h"
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "mipdebug.h"

#define MIPOSSINPUTOUTPUT_ERRSTR_ALREADYOPENED			"A device was already opened"
#define MIPOSSINPUTOUTPUT_ERRSTR_NOTOPENED			"No device was opened"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTOPENDEVICE			"Can't open device"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTGETFLAGS			"Can't read device flags"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSETFLAGS			"Can't write device flags"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSETFRAGMENTS		"Can't adjust the number of fragments or DMA block size"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTRESET			"Can't reset the sound device"
#define MIPOSSINPUTOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS		"Unsupported number of channels"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSETCHANNELS		"Can't set number of channels"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSETSIXTEENBITSAMPLES	"Can't set 16-bit samples"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSETSAMPLINGRATE		"Error setting sampling rate"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSETEXACTRATE		"Can't set exact sampling rate: rate set was "
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSETENCODING		"Error setting the sample encoding to 16-bit unsigned little endian samples"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSTARTINPUTTHREAD		"Can't start input thread"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTSTARTOUTPUTTHREAD		"Can't start output thread"
#define MIPOSSINPUTOUTPUT_ERRSTR_BADMESSAGE			"Can't understand message"
#define MIPOSSINPUTOUTPUT_ERRSTR_BUFFERTOOSMALL			"The specified buffer length is too small"
#define MIPOSSINPUTOUTPUT_ERRSTR_OUTPUTTHREADNOTRUNNING		"The output thread is not running"
#define MIPOSSINPUTOUTPUT_ERRSTR_INPUTTHREADNOTRUNNING		"The output thread is not running"
#define MIPOSSINPUTOUTPUT_ERRSTR_INCOMPATIBLECHANNELS		"Incompatible number of channels"
#define MIPOSSINPUTOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTINITSIGWAIT		"Can't initialize signal waiter"
#define MIPOSSINPUTOUTPUT_ERRSTR_DEVICENOTOPENEDFORREADING	"Device was not opened for reading"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTGETSUPPORTEDENCODINGS	"Can't obtain the supported sample formats"
#define MIPOSSINPUTOUTPUT_ERRSTR_NOSUPPORTEDENCODING		"The device doesn't support 16 bit signed or unsigned little endian data"
#define MIPOSSINPUTOUTPUT_ERRSTR_CANTGETCAPABILITIES		"Can't get the device capabilities"
#define MIPOSSINPUTOUTPUT_ERRSTR_NOFULLDUPLEX			"The device doesn't support full-duplex mode"

MIPOSSInputOutput::MIPOSSInputOutput() : MIPComponent("MIPOSSInputOutput"), m_blockTime(0), m_outputDistTime(0)
{
	m_device = -1;
	
	int status;
	
	if ((status = m_outputFrameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize OSS output frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_inputFrameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize OSS input frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPOSSInputOutput::~MIPOSSInputOutput()
{
	close();
}

bool MIPOSSInputOutput::open(int sampRate, int channels, MIPTime interval, AccessMode accessMode, 
                             const MIPOSSInputOutputParams *pParams)
{
	if (m_device != -1)
	{
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_ALREADYOPENED);
		return false;
	}
	
	if (!(channels == 1 || channels == 2))
	{
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS);
		return false;
	}

	MIPOSSInputOutputParams defaultParams;
	const MIPOSSInputOutputParams *pIOParams;

	if (pParams)
		pIOParams = pParams;
	else
		pIOParams = &defaultParams;

	std::string devName = pIOParams->getDeviceName();
	
	if (accessMode == ReadOnly)
		m_device = ::open(devName.c_str(),O_RDONLY|O_NONBLOCK);
	else if (accessMode == WriteOnly)
		m_device = ::open(devName.c_str(),O_WRONLY|O_NONBLOCK);
	else
		m_device = ::open(devName.c_str(),O_RDWR|O_NONBLOCK);
	
	if (m_device < 0)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}

	// Now that we've opened the file, we can set the blocking
	// mode again
	int flags = fcntl(m_device,F_GETFL);
	if (flags < 0)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTGETFLAGS);
		return false;
	}
	flags &= ~O_NONBLOCK; // disable non-blocking flags
	if (fcntl(m_device,F_SETFL,flags) < 0)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETFLAGS);
		return false;
	}

	int val;
	
	real_t bytesPerSecond = 2.0*(real_t)sampRate; // 16 bit samples

	if (channels == 2)
		bytesPerSecond *= 2.0;

	real_t ossBufTime = pIOParams->getOSSBufferSize().getValue();
	uint16_t ossFragments = pIOParams->getOSSFragments();
	
	
	if (ossBufTime < 0.001)
		ossBufTime = 0.001;
	if (ossBufTime > 1.000)
		ossBufTime = 1.000;

	if (ossFragments < 2)
		ossFragments = 2;
	if (ossFragments > 256)
		ossFragments = 256;
		
	// get amount of bytes in requested milliseconds
	
	real_t numBytes = ossBufTime*bytesPerSecond;
	int shiftNum = (int)(log(numBytes)/log(2)+0.5);
	
	if (shiftNum > 16)
		shiftNum = 16;
	else if (shiftNum < 8)
		shiftNum = 8;

	// set internal dma block size and number of fragments: this should
	// correspond to something like 42 milliseconds
	val = shiftNum | ((uint32_t)ossFragments<<16);
	if (ioctl(m_device,SNDCTL_DSP_SETFRAGMENT,&val) < 0)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETFRAGMENTS);
		return false;
	}

	if (accessMode == ReadWrite)
	{
		// check if full duplex is supported
		val = 0;
		if (ioctl(m_device,SNDCTL_DSP_GETCAPS,&val) < 0)
		{
			::close(m_device); 
			m_device = -1;
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTGETCAPABILITIES);
			return false;
		}
		if (!(val&DSP_CAP_DUPLEX))
		{
			::close(m_device); 
			m_device = -1;
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_NOFULLDUPLEX);
			return false;
		}
	}

	// check the supported sample encodings

	val = 0;
	if (ioctl(m_device,SNDCTL_DSP_GETFMTS,&val) < 0)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTGETSUPPORTEDENCODINGS);
		return false;
	}

	if (val&AFMT_S16_LE)
	{
		m_audioSubtype = MIPRAWAUDIOMESSAGE_TYPE_S16LE;
		m_isSigned = true;
	
		val = AFMT_S16_LE;
		if (ioctl(m_device,SNDCTL_DSP_SETFMT,&val) < 0)
		{
			::close(m_device); 
			m_device = -1;
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETENCODING);
			return false;
		}
	}
	else if (val&AFMT_U16_LE)
	{
		m_audioSubtype = MIPRAWAUDIOMESSAGE_TYPE_U16LE;
		m_isSigned = false;
	
		val = AFMT_U16_LE;
		if (ioctl(m_device,SNDCTL_DSP_SETFMT,&val) < 0)
		{
			::close(m_device); 
			m_device = -1;
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETENCODING);
			return false;
		}
	}
	else
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_NOSUPPORTEDENCODING);
		return false;
	}

	// set to stereo if required
	val = channels;
	if (ioctl(m_device,SNDCTL_DSP_CHANNELS,&val) < 0)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETCHANNELS);
		return false;
	}

	if (val != channels)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETCHANNELS);
		return false;
	}
	
	// set sampling rate
	bool exactRate = pIOParams->useExactRate();
	int realSamplingRate = sampRate;
	if (ioctl(m_device,SNDCTL_DSP_SPEED,&realSamplingRate) < 0)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETSAMPLINGRATE);
		return false;
	}

	if (exactRate && realSamplingRate != sampRate)
	{
		::close(m_device); 
		m_device = -1;

		char rateStr[256];
		
		MIP_SNPRINTF(rateStr, 255, "%d",realSamplingRate);
		setErrorString(std::string(MIPOSSINPUTOUTPUT_ERRSTR_CANTSETEXACTRATE)+std::string(rateStr));
		return false;
	}

	m_sampRate = realSamplingRate;

	// prepare buffers

	MIPTime bufferTime = pIOParams->getBufferTime();
	
	m_bufferLength = ((size_t)(bufferTime.getValue() * ((real_t)m_sampRate) + 0.5)) * channels;
	m_blockFrames = (size_t)(interval.getValue()*((real_t)m_sampRate) + 0.5);
	m_blockLength = m_blockFrames * channels;

	if (m_blockLength > m_bufferLength/4)
	{
		::close(m_device); 
		m_device = -1;
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_BUFFERTOOSMALL);
		return false;
	}

	size_t num = (size_t)(((real_t)m_bufferLength)/((real_t)m_blockLength)+0.5);
	
	m_bufferLength = m_blockLength * num;
	
	m_pInputBuffer = 0;
	m_pLastInputCopy = 0;
	m_pMsgBuffer = 0;
	m_pMsg = 0;
	m_pOutputBuffer = 0;
	m_pInputThread = 0;
	m_pOutputThread = 0;
	
	m_accessMode = accessMode;
	m_channels = channels;

	m_curOutputPos = 0;
	m_nextOutputPos = m_blockLength;
	m_outputDistTime = interval;
	m_blockTime = interval;

	uint16_t initVal;

	if (!m_isSigned)
	{
#ifndef MIPCONFIG_BIGENDIAN
		initVal = 0x8000;
#else
		initVal = 0x0080;
#endif // MIPCONFIG_BIGENDIAN
	}
	else
		initVal = 0;
	
	if (accessMode == ReadOnly || accessMode == ReadWrite)
	{
		if (!m_sigWait.init())
		{
			::close(m_device);
			m_device = -1;
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTINITSIGWAIT);
			return false;
		}
		
		m_pInputBuffer = new uint16_t [m_blockLength];
		m_pLastInputCopy = new uint16_t [m_blockLength];
		m_pMsgBuffer = new uint16_t [m_blockLength];
		for (size_t i = 0 ; i < m_blockLength ; i++)
		{
			m_pInputBuffer[i] = initVal;
			m_pLastInputCopy[i] = initVal;
			m_pMsgBuffer[i] = initVal;
		}
		m_gotLastInput = false;
		m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_channels, m_blockFrames, m_isSigned, MIPRaw16bitAudioMessage::LittleEndian, m_pMsgBuffer, false);
		
		m_pInputThread = new InputThread(*this);
		if (m_pInputThread->Start() < 0)
		{
			::close(m_device); 
			m_device = -1;
			delete [] m_pInputBuffer;
			delete [] m_pLastInputCopy;
			m_sigWait.destroy();
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSTARTINPUTTHREAD);
			return false;
		}
	}

	if (accessMode == WriteOnly || accessMode == ReadWrite)
	{
		m_pOutputBuffer = new uint16_t [m_bufferLength];
		for (size_t i = 0 ; i < m_bufferLength ; i++)
			m_pOutputBuffer[i] = initVal;

		m_pOutputThread = new OutputThread(*this);
		if (m_pOutputThread->Start() < 0)
		{
			delete [] m_pOutputBuffer;
			if (m_pInputThread)
				delete m_pInputThread;
			if (m_pInputBuffer)
				delete [] m_pInputBuffer;
			if (m_pLastInputCopy)
				delete [] m_pLastInputCopy;
			if (m_pMsgBuffer)
				delete [] m_pMsgBuffer;
			if (m_pMsg)
				delete m_pMsg;
			
			::close(m_device); 
			m_device = -1;
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_CANTSTARTOUTPUTTHREAD);
			return false;
		}
	}	

	return true;
}

bool MIPOSSInputOutput::close()
{
	if (m_device == -1)
	{
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}

	if (m_pInputThread)
		delete m_pInputThread;
	if (m_pOutputThread)
		delete m_pOutputThread;
	if (m_pInputBuffer)
		delete [] m_pInputBuffer;
	if (m_pLastInputCopy)
		delete [] m_pLastInputCopy;
	if (m_pMsgBuffer)
		delete [] m_pMsgBuffer;
	if (m_pMsg)
		delete m_pMsg;
	if (m_pOutputBuffer)
		delete [] m_pOutputBuffer;
	m_sigWait.destroy();
	::close(m_device);
	m_device = -1;
	
	return true;
}

bool MIPOSSInputOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_device == -1)
	{
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}
	
	if ((m_accessMode == ReadOnly || m_accessMode == ReadWrite) && 
	    pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && 
	    pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME)
	{
		m_inputFrameMutex.Lock();
		m_sigWait.clearSignalBuffers();
		bool gotFrame = m_gotLastInput;
		m_inputFrameMutex.Unlock();
		
		if (gotFrame) // already got the current frame in the buffer, wait for new one
			m_sigWait.waitForSignal();

		m_inputFrameMutex.Lock();
		memcpy(m_pMsgBuffer, m_pLastInputCopy, m_blockLength*sizeof(uint16_t));
		m_gotLastInput = true;
		m_pMsg->setTime(m_sampleInstant);
		m_inputFrameMutex.Unlock();
		
		m_gotMsg = false;
	}
	else if ((m_accessMode == WriteOnly || m_accessMode == ReadWrite) && 
	         pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW &&
		 pMsg->getMessageSubtype() == m_audioSubtype)
	{
		// Write block

		if (!m_pOutputThread->IsRunning())
		{
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_OUTPUTTHREADNOTRUNNING);
			return false;
		}
	
		MIPRaw16bitAudioMessage *audioMessage = (MIPRaw16bitAudioMessage *)pMsg;
	
		if (audioMessage->getSamplingRate() != m_sampRate)
		{
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
			return false;
		}
		if (audioMessage->getNumberOfChannels() != m_channels)
		{
			setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
			return false;
		}

		size_t num = audioMessage->getNumberOfFrames() * m_channels;
		size_t offset = 0;
		const uint16_t *frames = audioMessage->getFrames();

		m_outputFrameMutex.Lock();
		//std::cerr << "Adding at position " << m_nextOutputPos << std::endl;
		while (num > 0)
		{
			size_t maxAmount = m_bufferLength - m_nextOutputPos;
			size_t amount = (num > maxAmount)?maxAmount:num;
		
			if (m_nextOutputPos <= m_curOutputPos && m_nextOutputPos + amount > m_curOutputPos)
			{
//				std::cerr << "Strange error" << std::endl;
				m_nextOutputPos = m_curOutputPos + m_blockLength;
			}
		
			if (amount != 0)
				memcpy(m_pOutputBuffer + m_nextOutputPos, frames + offset, amount*sizeof(uint16_t));

			if (amount != num)
			{
				m_nextOutputPos = 0;
//				std::cerr << "Cycling next output pos" << std::endl;
			}
			else
				m_nextOutputPos += amount;
		
			offset += amount;
			num -= amount;
		}
		m_outputDistTime += MIPTime(((real_t)audioMessage->getNumberOfFrames())/((real_t)m_sampRate));
		m_outputFrameMutex.Unlock();
	}
	else
	{
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPOSSInputOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_device == -1)
	{
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}
	if (!(m_accessMode == ReadOnly || m_accessMode == ReadWrite))
	{
		setErrorString(MIPOSSINPUTOUTPUT_ERRSTR_DEVICENOTOPENEDFORREADING);
		return false;
	}

	if (m_gotMsg)
	{
		m_gotMsg = false;
		*pMsg = 0;
	}
	else
	{
		m_gotMsg = true;
		*pMsg = m_pMsg;
	}
	return true;
}

bool MIPOSSInputOutput::inputThreadStep()
{
	read(m_device, (void *)m_pInputBuffer, m_blockLength*sizeof(uint16_t));
	
	m_inputFrameMutex.Lock();
	m_gotLastInput = false;
	memcpy(m_pLastInputCopy, m_pInputBuffer, m_blockLength*sizeof(uint16_t));
	m_sampleInstant = MIPTime::getCurrentTime();
	m_sampleInstant -= m_blockTime;
	m_inputFrameMutex.Unlock();
	m_sigWait.signal();

	return true;
}

bool MIPOSSInputOutput::outputThreadStep()
{
	write(m_device, (void *)(m_pOutputBuffer + m_curOutputPos), m_blockLength*sizeof(uint16_t));
		
	m_outputFrameMutex.Lock();
	m_curOutputPos += m_blockLength;
	m_outputDistTime -= m_blockTime;
	if (m_curOutputPos + m_blockLength > m_bufferLength)
	{
		m_curOutputPos = 0;
	//	std::cerr << "Cycling cur output pos" << std::endl;
	}
		
	if (m_nextOutputPos < m_curOutputPos + m_blockLength && m_nextOutputPos >= m_curOutputPos)
	{
		m_nextOutputPos = m_curOutputPos + m_blockLength;
		if (m_nextOutputPos >= m_bufferLength)
			m_nextOutputPos = 0;
		m_outputDistTime = m_blockTime;
	//	std::cerr << "Pushing next output pos" << std::endl;
	}
		
	if (m_outputDistTime > MIPTime(0.500))
	{
	//	std::cerr << "Adjusting to runaway input (" << m_outputDistTime.getString() << ")" << std::endl;
		m_nextOutputPos = m_curOutputPos + m_blockLength;
		if (m_nextOutputPos >= m_bufferLength)
			m_nextOutputPos = 0;
		m_outputDistTime = m_blockTime;
	}
//	std::cerr << "About to write position " << m_curOutputPos << std::endl;
	m_outputFrameMutex.Unlock();
	return true;
}

MIPOSSInputOutput::IOThread::IOThread(MIPOSSInputOutput &ossIO) : m_ossIO(ossIO)
{
	int status;

	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize OSS output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	m_stopLoop = false;
}

MIPOSSInputOutput::IOThread::~IOThread()
{
	stop();
}

void MIPOSSInputOutput::IOThread::stop()
{
	MIPTime startTime = MIPTime::getCurrentTime();

	m_stopMutex.Lock();
	m_stopLoop = true;
	m_stopMutex.Unlock();
	
	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue() - startTime.getValue()) < 5.0)
		MIPTime::wait(MIPTime(0.020));

	if (JThread::IsRunning())
		JThread::Kill();
}

void *MIPOSSInputOutput::OutputThread::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPOSSInputOutput::OutputThread started" << std::endl;
#endif // MIPDEBUG

	JThread::ThreadStarted();	
	
	bool done;

	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();
	while (!done)
	{
		if (!m_ossIO.outputThreadStep())
			done = true;

		if (!done)
		{
			m_stopMutex.Lock();
			done = m_stopLoop;
			m_stopMutex.Unlock();
		}
	}
#ifdef MIPDEBUG
	std::cout << "MIPOSSInputOutput::OutputThread stopped" << std::endl;
#endif // MIPDEBUG
	return 0;
}

void *MIPOSSInputOutput::InputThread::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPOSSInputOutput::InputThread started" << std::endl;
#endif // MIPDEBUG
	
	JThread::ThreadStarted();	
	
	bool done;

	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();
	while (!done)
	{
		if (!m_ossIO.inputThreadStep())
			done = true;
		
		if (!done)
		{
			m_stopMutex.Lock();
			done = m_stopLoop;
			m_stopMutex.Unlock();
		}
	}
#ifdef MIPDEBUG
	std::cout << "MIPOSSInputOutput::InputThread stopped" << std::endl;
#endif // MIPDEBUG
	return 0;
}

#endif // MIPCONFIG_SUPPORT_OSS

