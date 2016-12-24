/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_WINMM

#include "mipwinmminput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include <iostream>

#include "mipdebug.h"

#define MIPWINMMINPUT_ERRSTR_ALREADYOPENED			"A device was already opened"
#define MIPWINMMINPUT_ERRSTR_NOTOPENED			"No device was opened"
#define MIPWINMMINPUT_ERRSTR_CANTOPENDEVICE			"Can't open device"
#define MIPWINMMINPUT_ERRSTR_CANTRESET			"Can't reset the sound device"
#define MIPWINMMINPUT_ERRSTR_UNSUPPORTEDCHANNELS		"Unsupported number of channels"
#define MIPWINMMINPUT_ERRSTR_CANTSTARTINPUTTHREAD		"Can't start input thread"
#define MIPWINMMINPUT_ERRSTR_BADMESSAGE			"Can't understand message"
#define MIPWINMMINPUT_ERRSTR_BUFFERTOOSMALL			"The specified buffer length is too small"
#define MIPWINMMINPUT_ERRSTR_INCOMPATIBLECHANNELS		"Incompatible number of channels"
#define MIPWINMMINPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPWINMMINPUT_ERRSTR_CANTINITSIGWAIT		"Can't initialize signal waiter"
#define MIPWINMMINPUT_ERRSTR_CANTUNPREPARE			"Can't unprepare header"
#define MIPWINMMINPUT_ERRSTR_CANTCLOSE				"Can't close device"
#define MIPWINMMINPUT_ERRSTR_THREADERROR			"Error in thread: "

MIPWinMMInput::MIPWinMMInput() : MIPComponent("MIPWinMMInput")
{
	m_init = false;
	
	int status;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize WinMMInput frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize WinMMInput stop mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPWinMMInput::~MIPWinMMInput()
{
	close();
}

bool MIPWinMMInput::open(int sampRate, int channels, MIPTime interval, MIPTime bufferTime, bool highPriority, UINT deviceID)
{
	if (m_init)
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_ALREADYOPENED);
		return false;
	}

	int fact;
	WAVEFORMATEX format;

	fact = 1;
	fact *= channels;
	fact *= 2; // we'll use two bytes per sample

	format.nAvgBytesPerSec = sampRate*fact;
	format.nBlockAlign = fact;
	format.nChannels = channels;
	format.nSamplesPerSec = sampRate;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.cbSize = 0;
	format.wBitsPerSample = 2*8; // two bytes per sample

	if (waveInOpen((LPHWAVEIN)&m_device, deviceID, (LPWAVEFORMATEX)&format,(DWORD_PTR)inputCallback,(DWORD_PTR)this,CALLBACK_FUNCTION))
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}

	m_sampRate = sampRate;
	m_channels = channels;

	if (!m_sigWait.init())
	{
		waveInClose(m_device);
		setErrorString(MIPWINMMINPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}
	if (!m_threadSigWait.init())
	{
		m_sigWait.destroy();
		waveInClose(m_device);
		setErrorString(MIPWINMMINPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}

	m_frames = ((size_t)(interval.getValue() * ((real_t)sampRate) + 0.5));
	m_bufSize = m_frames * m_channels * sizeof(uint16_t);

	size_t blockLength = m_frames*m_channels;

	m_pLastInputCopy = new uint16_t [blockLength];
	m_pMsgBuffer = new uint16_t [blockLength];

	for (size_t i = 0 ; i < blockLength ; i++)
	{
		m_pLastInputCopy[i] = 0;
		m_pMsgBuffer[i] = 0;
	}

	m_gotLastInput = false;
	m_gotMsg = false;
	m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_channels, (int)m_frames, true, MIPRaw16bitAudioMessage::LittleEndian, m_pMsgBuffer, false);
	m_stopThread = false;

	m_numBuffers = (int)((bufferTime.getValue() / interval.getValue())+0.5);

	if (m_numBuffers < 10)
		m_numBuffers = 10;

	m_inputBuffers = new WAVEHDR[m_numBuffers];

	if (!highPriority)
		m_threadPrioritySet = true;
	else
		m_threadPrioritySet = false;

	if (JThread::Start() < 0)
	{
		m_sigWait.destroy();
		m_threadSigWait.destroy();
		waveInClose(m_device);
		delete m_pMsg;
		delete [] m_pMsgBuffer;
		delete [] m_pLastInputCopy;
		delete [] m_inputBuffers;
		setErrorString(MIPWINMMINPUT_ERRSTR_CANTSTARTINPUTTHREAD);
		return false;
	}

	m_interval = interval;
	m_init = true;

	return true;
}

bool MIPWinMMInput::close()
{
	if (!m_init)
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_NOTOPENED);
		return false;
	}

	m_stopMutex.Lock();
	m_stopThread = true;
	m_stopMutex.Unlock();

	MIPTime startTime = MIPTime::getCurrentTime();

	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue() - startTime.getValue()) < 5.0)
		MIPTime::wait(MIPTime(0.050));

	if (JThread::IsRunning())
		JThread::Kill();

	if (waveInReset(m_device) != MMSYSERR_NOERROR)
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_CANTRESET);
		return false;
	}
	for (int i = 0 ; i < m_numBuffers ; i++)
	{
		if (waveInUnprepareHeader(m_device, &m_inputBuffers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			setErrorString(MIPWINMMINPUT_ERRSTR_CANTUNPREPARE);
			return false;
		}
		uint16_t *pBuf = (uint16_t *)m_inputBuffers[i].lpData;
		delete [] pBuf;
	}

	if (waveInClose(m_device) != MMSYSERR_NOERROR)
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_CANTCLOSE);
		return false;
	}

	delete m_pMsg;
	delete [] m_pLastInputCopy;
	delete [] m_pMsgBuffer;
	delete [] m_inputBuffers;

	m_threadSigWait.destroy();
	m_sigWait.destroy();

	m_init = false;

	return true;
}

bool MIPWinMMInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_NOTOPENED);
		return false;
	}

	if (!m_threadPrioritySet)
	{
		m_threadPrioritySet = true;
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	}

	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME)
	{
		if (!JThread::IsRunning())
		{
			setErrorString(std::string(MIPWINMMINPUT_ERRSTR_THREADERROR)+m_threadError);
			return false;
		}

		m_frameMutex.Lock();
		m_sigWait.clearSignalBuffers();
		bool gotFrame = m_gotLastInput;
		m_frameMutex.Unlock();
		
		if (gotFrame) // already got the current frame in the buffer, wait for new one
			m_sigWait.waitForSignal();

		m_frameMutex.Lock();

		int num = ((int)m_frames)*m_channels;

		memcpy(m_pMsgBuffer, m_pLastInputCopy, num*sizeof(uint16_t));
		
		m_gotLastInput = true;
		m_pMsg->setTime(m_sampleInstant);

		m_frameMutex.Unlock();
		
		m_gotMsg = false;
	}
	else
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_BADMESSAGE);
		return false;
	}

	return true;
}

bool MIPWinMMInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPWINMMINPUT_ERRSTR_NOTOPENED);
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

// the thread makes sure that the input keeps getting filled
void *MIPWinMMInput::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPWinMMInput::Thread started" << std::endl;
#endif // MIPDEBUG

	if (!m_threadPrioritySet)
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	
	JThread::ThreadStarted();

	m_threadError = std::string("");

	for (int i = 0 ; i < m_numBuffers ; i++) // prepare the buffers
	{
		m_inputBuffers[i].dwBufferLength = (DWORD)m_bufSize;
		m_inputBuffers[i].dwFlags = 0;
		m_inputBuffers[i].dwLoops = 0;
		m_inputBuffers[i].dwUser = 0;
		m_inputBuffers[i].dwBytesRecorded = 0;
		m_inputBuffers[i].lpData = (LPSTR) new uint16_t[m_frames*m_channels];
		
		if (waveInPrepareHeader(m_device,&m_inputBuffers[i],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			m_threadError = std::string("Error preparing initial headers");
			return 0;
		}
		if (waveInAddBuffer(m_device,&m_inputBuffers[i],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			m_threadError = std::string("Error adding initial buffers");
			return 0;
		}
	}

	if (waveInStart(m_device) != MMSYSERR_NOERROR)
		return 0;

	bool done = false;

	m_stopMutex.Lock();
	done = m_stopThread;
	m_stopMutex.Unlock();

	int curpos = 0;

	while(!done)
	{
		m_threadSigWait.waitForSignal();
		if (waveInUnprepareHeader(m_device,&m_inputBuffers[curpos],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			m_threadError = std::string("Error unpreparing header");
			return 0;
		}
		m_inputBuffers[curpos].dwFlags = 0;
		m_inputBuffers[curpos].dwLoops = 0;
		m_inputBuffers[curpos].dwUser = 0;
		m_inputBuffers[curpos].dwBytesRecorded = 0;

		if (waveInPrepareHeader(m_device,&m_inputBuffers[curpos],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			m_threadError = std::string("Error preparing header");
			return 0;
		}
		if (waveInAddBuffer(m_device,&m_inputBuffers[curpos],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			m_threadError = std::string("Error adding buffer");
			return 0;
		}

		curpos++;
		if (curpos == m_numBuffers)
			curpos = 0;

		m_stopMutex.Lock();
		done = m_stopThread;
		m_stopMutex.Unlock();
	}
	
#ifdef MIPDEBUG
	std::cout << "MIPWinMMInput::Thread stopped" << std::endl;
#endif // MIPDEBUG

	return 0;
}

void CALLBACK MIPWinMMInput::inputCallback(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg == WIM_DATA)
	{
		MIPWinMMInput *pInput = (MIPWinMMInput *)dwInstance;
		LPWAVEHDR hdr = (LPWAVEHDR)dwParam1;

		pInput->m_frameMutex.Lock();
		memcpy(pInput->m_pLastInputCopy,hdr->lpData,hdr->dwBufferLength);
		pInput->m_gotLastInput = false;
		pInput->m_sampleInstant = MIPTime::getCurrentTime();
		pInput->m_sampleInstant -= pInput->m_interval;
		pInput->m_frameMutex.Unlock();

		pInput->m_sigWait.signal();
		pInput->m_threadSigWait.signal();
	}
}

#endif // MIPCONFIG_SUPPORT_WINMM

