/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_JACK

#include "mipjackinput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include <iostream>

#include "mipdebug.h"

#define MIPJACKINPUT_ERRSTR_NOTOPENED				"Client not opened yet"
#define MIPJACKINPUT_ERRSTR_BADMESSAGE				"Bad message"
#define MIPJACKINPUT_ERRSTR_CLIENTALREADYOPEN			"Client is already connected"
#define MIPJACKINPUT_ERRSTR_CANTOPENCLIENT			"Can't open client"
#define MIPJACKINPUT_ERRSTR_CANTREGISTERCHANNEL			"Can't register a channel"
#define MIPJACKINPUT_ERRSTR_CANTINITSIGWAIT			"Can't initialize signal waiter"
#define MIPJACKINPUT_ERRSTR_CANTACTIVATECLIENT			"Can't activate the client"
#define MIPJACKINPUT_ERRSTR_CANTGETPORTS			"Can't get physical ports"
#define MIPJACKINPUT_ERRSTR_NOTENOUGHPORTS			"Not enough physical ports were found: stereo sound is required"
#define MIPJACKINPUT_ERRSTR_CANTCONNECTPORTS			"Can't connect ports"
#define MIPJACKINPUT_ERRSTR_INTERVALTOOSMALL			"Specified interval is too small"
#define MIPJACKINPUT_ERRSTR_ARRAYSIZETOOSMALL			"Specified array time is too small"
#define MIPJACKINPUT_ERRSTR_CANTSTARTTHREAD			"Can't start thread"

MIPJackInput::MIPJackInput() : MIPComponent("MIPJackInput")
{
	m_pClient = 0;
	
	int status;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize Jack input frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_blockMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize Jack input block mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize Jack input stop mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPJackInput::~MIPJackInput()
{
	close();
}

bool MIPJackInput::open(MIPTime interval, const std::string &serverName, MIPTime arrayTime)
{
	if (m_pClient != 0)
	{
		setErrorString(MIPJACKINPUT_ERRSTR_CLIENTALREADYOPEN);
		return false;
	}
	
	std::string clientName("MIPJackInput");
	const char *pServName = 0;
	jack_options_t options = JackNullOption;
	jack_status_t jackStatus;
	
	if (serverName.length() != 0)
	{
		pServName = serverName.c_str();
		options = (jack_options_t)((int)options|(int)JackServerName);
	}

	m_pClient = jack_client_open(clientName.c_str(), options, &jackStatus, pServName);
	if (m_pClient == 0)
	{
		setErrorString(MIPJACKINPUT_ERRSTR_CANTOPENCLIENT);
		return false;
	}
	
	jack_set_process_callback (m_pClient, StaticJackCallback, this);

	m_pLeftInput = jack_port_register(m_pClient, "leftChannel", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	m_pRightInput = jack_port_register(m_pClient, "rightChannel", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if (m_pLeftInput == 0 || m_pRightInput == 0)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		setErrorString(MIPJACKINPUT_ERRSTR_CANTREGISTERCHANNEL);
		return false;
	}

	m_sampRate = jack_get_sample_rate(m_pClient);
	m_blockSize = jack_get_buffer_size(m_pClient);
	m_frames = (size_t)(interval.getValue()*(real_t)m_sampRate+0.5);
	m_bufSize = 2*m_frames; // for stereo

	if (m_frames < m_blockSize)
	{
		// Attempt to set a better suited blocksize

		size_t s = 1;

		while (s < m_frames)
			s <<= 1;

		s >>= 1;

		if (s > 0)
		{
			if (jack_set_buffer_size(m_pClient, s) != 0)
			{
				jack_client_close(m_pClient);
				m_pClient = 0;
				setErrorString(MIPJACKINPUT_ERRSTR_INTERVALTOOSMALL);
				return false;
			}
			m_blockSize = jack_get_buffer_size(m_pClient);
			if (m_frames < m_blockSize) // double check
			{
				jack_client_close(m_pClient);
				m_pClient = 0;
				setErrorString(MIPJACKINPUT_ERRSTR_INTERVALTOOSMALL);
				return false;
			}
		}
		else
		{
			jack_client_close(m_pClient);
			m_pClient = 0;
			setErrorString(MIPJACKINPUT_ERRSTR_INTERVALTOOSMALL);
			return false;
		}
	}
	
	m_curBuffer = 0;
	m_curReadBuffer = 0;
	m_availableFrames = 0;

	size_t arraySize = (size_t)(arrayTime.getValue()*(real_t)m_sampRate+0.5);
	if (arraySize < m_bufSize/2*10)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		setErrorString(MIPJACKINPUT_ERRSTR_ARRAYSIZETOOSMALL);
		return false;
	}

	m_numBuffers = arraySize/m_blockSize+1;
	m_pRecordBuffers = new RecordBuffer* [m_numBuffers];
	for (int i = 0 ; i < m_numBuffers ; i++)
		m_pRecordBuffers[i] = new RecordBuffer(m_blockSize);
		
	m_pLastInputCopy = new float [m_bufSize];
	m_pMsgBuffer = new float [m_bufSize];
	m_interval = interval;
	m_gotLastInput = false;
	m_gotMsg = false;
	
	memset(m_pLastInputCopy, 0, sizeof(float)*m_bufSize);
	memset(m_pMsgBuffer, 0, sizeof(float)*m_bufSize);

	m_pMsg = new MIPRawFloatAudioMessage(m_sampRate, 2, m_bufSize/2, m_pMsgBuffer, true);
	
	if (!m_sigWait.init())
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPJACKINPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}
	if (!m_sigWait2.init())
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		m_sigWait.destroy();
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPJACKINPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}

	if (jack_activate(m_pClient) != 0)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		m_sigWait.destroy();
		m_sigWait2.destroy();
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPJACKINPUT_ERRSTR_CANTACTIVATECLIENT);
		return false;
	}

	const char **pPhysPorts = jack_get_ports(m_pClient, 0, 0, JackPortIsPhysical|JackPortIsOutput);
	if (pPhysPorts == 0)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		m_sigWait.destroy();
		m_sigWait2.destroy();
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPJACKINPUT_ERRSTR_CANTGETPORTS);
		return false;
	}
	
	for (int i = 0 ; i < 2 ; i++)
	{
		if (pPhysPorts[i] == 0)
		{
			free(pPhysPorts);
			jack_client_close(m_pClient);
			m_pClient = 0;
			delete [] m_pLastInputCopy;
			delete m_pMsg;
			m_sigWait.destroy();
			m_sigWait2.destroy();
			for (int i = 0 ; i < m_numBuffers ; i++)
				delete m_pRecordBuffers[i];
			delete [] m_pRecordBuffers;
			setErrorString(MIPJACKINPUT_ERRSTR_NOTENOUGHPORTS);
			return false;
		}

		jack_port_t *pSrc = 0;
		
		if (i == 0)
			pSrc = m_pLeftInput;
		else
			pSrc = m_pRightInput;
			
		if (jack_connect(m_pClient, pPhysPorts[i], jack_port_name(pSrc)) != 0)
		{
			free(pPhysPorts);
			jack_client_close(m_pClient);
			m_pClient = 0;
			delete [] m_pLastInputCopy;
			delete m_pMsg;
			m_sigWait.destroy();
			m_sigWait2.destroy();
			for (int i = 0 ; i < m_numBuffers ; i++)
				delete m_pRecordBuffers[i];
			delete [] m_pRecordBuffers;
			setErrorString(MIPJACKINPUT_ERRSTR_CANTCONNECTPORTS);
			return false;
		}
	}
			
	free(pPhysPorts);

	m_stopThread = false;
	if (JThread::Start() < 0)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		m_sigWait.destroy();
		m_sigWait2.destroy();
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPJACKINPUT_ERRSTR_CANTSTARTTHREAD);
		return false;
	}
	
	return true;
}

bool MIPJackInput::close()
{
	if (m_pClient == 0)
	{
		setErrorString(MIPJACKINPUT_ERRSTR_NOTOPENED);
		return false;
	}

	jack_client_close(m_pClient);
	m_pClient = 0;
	
	m_stopMutex.Lock();
	m_stopThread = true;
	m_stopMutex.Unlock();

	m_sigWait2.signal();
	
	MIPTime startTime = MIPTime::getCurrentTime();
	
	while (JThread::IsRunning() && MIPTime::getCurrentTime().getValue() - startTime.getValue() < 5.0)
	{
		MIPTime::wait(MIPTime(0.010));
	}

	if (JThread::IsRunning())
		JThread::Kill();
	
	m_sigWait.destroy();
	m_sigWait2.destroy();
	delete [] m_pLastInputCopy;
	delete m_pMsg;
	for (int i = 0 ; i < m_numBuffers ; i++)
		delete m_pRecordBuffers[i];
	delete [] m_pRecordBuffers;

	return true;
}

bool MIPJackInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pClient == 0)
	{
		setErrorString(MIPJACKINPUT_ERRSTR_NOTOPENED);
		return false;
	}

	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME)
	{
		m_frameMutex.Lock();
		m_sigWait.clearSignalBuffers();
		bool gotFrame = m_gotLastInput;
		m_frameMutex.Unlock();
		
		if (gotFrame) // already got the current frame in the buffer, wait for new one
			m_sigWait.waitForSignal();

		m_frameMutex.Lock();

		memcpy(m_pMsgBuffer, m_pLastInputCopy, m_bufSize*sizeof(float));
		
		m_gotLastInput = true;
		m_pMsg->setTime(m_sampleInstant);

		m_frameMutex.Unlock();
		
		m_gotMsg = false;
	}
	else
	{
		setErrorString(MIPJACKINPUT_ERRSTR_BADMESSAGE);
		return false;
	}

	return true;
}

bool MIPJackInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pClient == 0)
	{
		setErrorString(MIPJACKINPUT_ERRSTR_NOTOPENED);
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

int MIPJackInput::StaticJackCallback(jack_nframes_t nframes, void *arg)
{
	MIPJackInput *pInstance = (MIPJackInput *)arg;
	return pInstance->JackCallback(nframes);
}

int MIPJackInput::JackCallback(jack_nframes_t nframes)
{
	if (nframes > m_blockSize)
		nframes = m_blockSize; // make sure buffer size changes don't cause a crash.
	
	void *pLeftBuf = jack_port_get_buffer(m_pLeftInput, nframes);
	void *pRightBuf = jack_port_get_buffer(m_pRightInput, nframes);
	RecordBuffer *pBuf = m_pRecordBuffers[m_curBuffer];
	
	memcpy(pBuf->getLeftBuffer(),pLeftBuf,nframes*sizeof(float));
	memcpy(pBuf->getRightBuffer(),pRightBuf,nframes*sizeof(float));
	pBuf->setOffset(0);

	m_curBuffer++;
	if (m_curBuffer == m_numBuffers)
		m_curBuffer = 0;

	m_blockMutex.Lock();
	m_availableFrames += (size_t)nframes;
	m_blockMutex.Unlock();

	m_sigWait2.signal();
	
	return 0;
}

void *MIPJackInput::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPJackInput::Thread started" << std::endl;
#endif // MIPDEBUG
	JThread::ThreadStarted();
	bool stop = false;
	size_t numFrames = m_frames;

	while (!stop)
	{
		m_sigWait2.waitForSignal();

		m_stopMutex.Lock();
		stop = m_stopThread;
		m_stopMutex.Unlock();

		if (!stop)
		{
			// new block is ready
			m_blockMutex.Lock();
			size_t avail = m_availableFrames;
			m_blockMutex.Unlock();

			while (avail >= numFrames)
			{
				size_t framesToGo = numFrames;
				int i = 0;
				
				m_frameMutex.Lock();
				while (framesToGo > 0)
				{
					size_t curOffset = m_pRecordBuffers[m_curReadBuffer]->getOffset();
					size_t maxNum = m_blockSize-curOffset;
					size_t num = framesToGo;
					size_t j,k;
					jack_default_audio_sample_t *pLeft = m_pRecordBuffers[m_curReadBuffer]->getLeftBuffer();
					jack_default_audio_sample_t *pRight = m_pRecordBuffers[m_curReadBuffer]->getRightBuffer();
					
					if (num > maxNum)
						num = maxNum;
					for (j = 0, k = curOffset ; j < num ; k++, j++)
					{
						m_pLastInputCopy[i++] = pLeft[k];
						m_pLastInputCopy[i++] = pRight[k];
					}

					framesToGo -= num;
					curOffset += num;
					if (curOffset >= m_blockSize)
					{
						m_pRecordBuffers[m_curReadBuffer]->setOffset(0);
						m_curReadBuffer++;
						if (m_curReadBuffer >= m_numBuffers)
							m_curReadBuffer = 0;
					}
					else
						m_pRecordBuffers[m_curReadBuffer]->setOffset(curOffset);
				}
				m_frameMutex.Unlock();
				
				m_sigWait.signal();
				
				m_blockMutex.Lock();
				m_availableFrames -= numFrames;
				avail = m_availableFrames;
				m_blockMutex.Unlock();
			}
		}
	}
#ifdef MIPDEBUG
	std::cout << "MIPJackInput::Thread started" << std::endl;
#endif // MIPDEBUG
	return 0;
}
#endif // MIPCONFIG_SUPPORT_JACK

