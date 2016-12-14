/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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

#ifdef MIPCONFIG_SUPPORT_ESD

#include "mipesdoutput.h"
#include "miprawaudiomessage.h"

#define MIPESDOUTPUT_ERRSTR_DEVICEALREADYOPEN		"Device already opened"
#define MIPESDOUTPUT_ERRSTR_DEVICENOTOPEN		"Device is not opened"
#define MIPESDOUTPUT_ERRSTR_CANTOPENDEVICE		"Error opening device"
#define MIPESDOUTPUT_ERRSTR_CANTSETINTERLEAVEDMODE	"Can't set device to interleaved mode"
#define MIPESDOUTPUT_ERRSTR_FLOATNOTSUPPORTED		"Floating point format is not supported by this device"
#define MIPESDOUTPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE	"The requested sampling rate is not supported"
#define MIPESDOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS		"The requested number of channels is not supported"
#define MIPESDOUTPUT_ERRSTR_BLOCKTIMETOOLARGE		"Block time too large"
#define MIPESDOUTPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD	"Can't start the background thread"
#define MIPESDOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"No pull available for this component"
#define MIPESDOUTPUT_ERRSTR_THREADSTOPPED		"Background thread stopped"
#define MIPESDOUTPUT_ERRSTR_BADMESSAGE			"Only raw audio messages are supported"
#define MIPESDOUTPUT_ERRSTR_INCOMPATIBLECHANNELS	"Incompatible number of channels"
#define MIPESDOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPESDOUTPUT_ERRSTR_BUFFERFULL			"Buffer full"
#define MIPESDOUTPUT_ERRSTR_UNKNOWNCHANNELS		"The ESD device used an unknown number of channels"
#define MIPESDOUTPUT_ERRSTR_UNSUPPORTEDBITS		"The ESD device does not use 16 bit samples"

MIPEsdOutput::MIPEsdOutput() : MIPComponent("MIPEsdOutput"), m_delay(0), m_distTime(0), m_blockTime(0)
{
	int status;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize ESD output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize ESD output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}

	m_opened = false;
}

MIPEsdOutput::~MIPEsdOutput()
{
	close();
}

bool MIPEsdOutput::open(MIPTime blockTime, MIPTime arrayTime)
{
	if (m_opened)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_DEVICEALREADYOPEN);
		return false;
	}
	
	int status = esd_audio_open();
	if (status < 0)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}

	m_sampRate = esd_audio_rate;
	esd_format_t fmt = esd_audio_format;

	if ((fmt & ESD_MASK_CHAN) == ESD_MONO)
		m_channels = 1;
	else if ((fmt & ESD_MASK_CHAN) == ESD_STEREO)
		m_channels = 2;
	else
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_UNKNOWNCHANNELS);
		return false;
	}

	if ((fmt & ESD_MASK_BITS) != ESD_BITS16)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_UNSUPPORTEDBITS);
		return false;
	}

	m_frameArrayLength = ((size_t)((arrayTime.getValue() * ((real_t)m_sampRate)) + 0.5)) * ((size_t)m_channels);
	m_blockFrames = ((size_t)((blockTime.getValue() * ((real_t)m_sampRate)) + 0.5));
	m_blockLength = m_blockFrames * ((size_t)m_channels);

	if (m_blockLength > m_frameArrayLength/4)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_BLOCKTIMETOOLARGE);
		return false;
	}

	// make sure a fixed number of blocks fit in the frame array
	size_t numBlocks = m_frameArrayLength / m_blockLength + 1;
	
	m_frameArrayLength = numBlocks * m_blockLength;
	m_pFrameArray = new uint16_t[m_frameArrayLength];
	for (size_t i = 0 ; i < m_frameArrayLength ; i++)
		m_pFrameArray[i] = 0;
	
	m_currentPos = 0;
	m_nextPos = m_blockLength;
	m_distTime = blockTime;
	m_blockTime = blockTime;

	m_stopLoop = false;
	if (JThread::Start() < 0)
	{
		delete [] m_pFrameArray;
		std::cerr << "error" << std::endl;
		setErrorString(MIPESDOUTPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD);
		return false;
	}
	m_opened = true;
	
	return true;
}

bool MIPEsdOutput::close()
{
	if (!m_opened)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	MIPTime starttime = MIPTime::getCurrentTime();
	
	m_stopMutex.Lock();
	m_stopLoop = true;
	m_stopMutex.Unlock();
	
	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue()-starttime.getValue()) < 5.0)
	{
		MIPTime::wait(MIPTime(0.010));
	}

	if (JThread::IsRunning())
		JThread::Kill();

	esd_audio_close();
	delete [] m_pFrameArray;
		
	return true;
}

bool MIPEsdOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_opened)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16))
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (!JThread::IsRunning())
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_THREADSTOPPED);
		return false;
	}

	MIPRaw16bitAudioMessage *audioMessage = (MIPRaw16bitAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}
	
	size_t num = audioMessage->getNumberOfFrames() * m_channels;
	size_t offset = 0;
	const uint16_t *frames = audioMessage->getFrames();

	m_frameMutex.Lock();
	while (num > 0)
	{
		size_t maxAmount = m_frameArrayLength - m_nextPos;
		size_t amount = (num > maxAmount)?maxAmount:num;
		
		if (m_nextPos <= m_currentPos && m_nextPos + amount > m_currentPos)
		{
			std::cerr << "Strange error" << std::endl;
			m_nextPos = m_currentPos + m_blockLength;
		}
		
		if (amount != 0)
			memcpy(m_pFrameArray + m_nextPos, frames + offset, amount*sizeof(uint16_t));

		if (amount != num)
		{
			m_nextPos = 0;
			//std::cerr << "Cycling next pos" << std::endl;
		}
		else
			m_nextPos += amount;
		
		offset += amount;
		num -= amount;
	}
	m_distTime += MIPTime(((real_t)audioMessage->getNumberOfFrames())/((real_t)m_sampRate));
	m_frameMutex.Unlock();
	
	return true;
}

bool MIPEsdOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPESDOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return false;
}

void *MIPEsdOutput::Thread()
{
	JThread::ThreadStarted();

	bool done;
	
	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();

	while (!done)
	{
		esd_audio_write(m_pFrameArray + m_currentPos, m_blockLength*sizeof(uint16_t));
		
		m_frameMutex.Lock();
		m_currentPos += m_blockLength;
		m_distTime -= m_blockTime;
		if (m_currentPos + m_blockLength > m_frameArrayLength)
		{
			m_currentPos = 0;
//			std::cerr << "Cycling" << std::endl;
		}
		
		if (m_nextPos < m_currentPos + m_blockLength && m_nextPos >= m_currentPos)
		{
			m_nextPos = m_currentPos + m_blockLength;
			if (m_nextPos >= m_frameArrayLength)
				m_nextPos = 0;
			m_distTime = m_blockTime;
			//std::cerr << "Pushing next position" << std::endl;
		}
		
		if (m_distTime > MIPTime(0.200))
		{
			m_nextPos = m_currentPos + m_blockLength;
			if (m_nextPos >= m_frameArrayLength)
				m_nextPos = 0;
			m_distTime = m_blockTime;
//			std::cerr << "Adjusting to runaway input (" << m_distTime.getString() << ")" << std::endl;
		}
		m_frameMutex.Unlock();
		
		m_stopMutex.Lock();
		done = m_stopLoop;
		m_stopMutex.Unlock();
	}

	return 0;
}

#endif // MIPCONFIG_SUPPORT_ESD

