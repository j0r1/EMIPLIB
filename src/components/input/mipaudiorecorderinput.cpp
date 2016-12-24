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

#ifdef MIPCONFIG_SUPPORT_AUDIORECORDER

#include "mipaudiorecorderinput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include <media/AudioRecord.h>

#include <iostream>

#include "mipdebug.h"

#define MIPAUDIORECORDERINPUT_ERRSTR_NOTOPENED				"Recorder not opened yet"
#define MIPAUDIORECORDERINPUT_ERRSTR_CANTINITRECORDER			"Can't initialize recorder"
#define MIPAUDIORECORDERINPUT_ERRSTR_ALREADYOPEN			"Recorder has already been opened"
#define MIPAUDIORECORDERINPUT_ERRSTR_BADMESSAGE				"Bad message"
#define MIPAUDIORECORDERINPUT_ERRSTR_CANTINITSIGWAIT			"Can't initialize signal waiter"
#define MIPAUDIORECORDERINPUT_ERRSTR_INTERVALTOOSMALL			"Specified interval is too small, should be larger than "
#define MIPAUDIORECORDERINPUT_ERRSTR_ARRAYSIZETOOSMALL			"Specified array time is too small"
#define MIPAUDIORECORDERINPUT_ERRSTR_CANTSTARTTHREAD			"Can't start thread"
#define MIPAUDIORECORDERINPUT_ERRSTR_CANTGETREQUESTEDSAMPLERATE		"Requested sampling rate was not obtained"
#define MIPAUDIORECORDERINPUT_ERRSTR_CANTGETREQUESTEDCHANNELS		"Requested number of channels was not obtained"
#define MIPAUDIORECORDERINPUT_ERRSTR_CANTSTARTRECORDER			"Unable to start the AudioRecord instance"
#define MIPAUDIORECORDERINPUT_ERRSTR_INVALIDNUMBEROFCHANNELS		"Number of channels should be 1 or 2"

MIPAudioRecorderInput::MIPAudioRecorderInput() : MIPComponent("MIPAudioRecorderInput")
{
	m_pRecorder = 0;
	
	int status;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize AudioRecorder input frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_blockMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize AudioRecorder input block mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize AudioRecorder input stop mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPAudioRecorderInput::~MIPAudioRecorderInput()
{
	close();
}

bool MIPAudioRecorderInput::open(int sampRate, int channels, MIPTime interval, MIPTime arrayTime)
{
	if (m_pRecorder != 0)
	{
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_ALREADYOPEN);
		return false;
	}
	
	if (!(channels == 1 || channels == 2))
	{
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_INVALIDNUMBEROFCHANNELS);
		return false;
	}

	m_pRecorder = new android::AudioRecord();
	if (m_pRecorder->set(android::AudioRecord::MIC_INPUT, sampRate, android::AudioSystem::PCM_16_BIT,
	                     channels, 0, 0, MIPAudioRecorderInput::StaticAudioRecorderCallback, this, 0) != 0)
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_CANTINITRECORDER);
		return false;
	}
	
	/*
	if (sampRate != m_pRecorder->getSampleRate())
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_CANTGETREQUESTEDSAMPLERATE);
		return false;
	}
	*/

	if (channels != m_pRecorder->channelCount())
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_CANTGETREQUESTEDCHANNELS);
		return false;
	}

	m_sampRate = sampRate;
	m_channels = channels;

	m_blockFrames = m_pRecorder->frameCount()/2; // TODO: Is there a way to obtain this value? (other than looking in the AudioRecord source code)
	m_blockSize = m_blockFrames*channels;

	m_frames = (size_t)(interval.getValue()*(real_t)m_sampRate+0.5);
	m_bufSize = channels*m_frames;

	if (m_frames < m_blockFrames)
	{
		delete m_pRecorder;
		m_pRecorder = 0;

		char str[256];
		real_t minInterval = ((real_t)m_blockFrames)/((real_t)m_sampRate);

		MIP_SNPRINTF(str, 255, "%f", (double)minInterval);

		setErrorString(std::string(MIPAUDIORECORDERINPUT_ERRSTR_INTERVALTOOSMALL) + std::string(str));
		return false;
	}

	m_curBuffer = 0;
	m_curReadBuffer = 0;
	m_availableFrames = 0;

	size_t arraySize = (size_t)(arrayTime.getValue()*(real_t)m_sampRate+0.5);
	if (arraySize < m_bufSize/2*10)
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_ARRAYSIZETOOSMALL);
		return false;
	}

	m_numBuffers = arraySize/m_blockSize+1;
	m_pRecordBuffers = new RecordBuffer* [m_numBuffers];
	for (int i = 0 ; i < m_numBuffers ; i++)
		m_pRecordBuffers[i] = new RecordBuffer(m_blockSize);
		
	m_pLastInputCopy = new uint16_t [m_bufSize];
	m_pMsgBuffer = new uint16_t [m_bufSize];
	m_interval = interval;
	m_gotLastInput = false;
	m_gotMsg = false;
	
	memset(m_pLastInputCopy, 0, sizeof(uint16_t)*m_bufSize);
	memset(m_pMsgBuffer, 0, sizeof(uint16_t)*m_bufSize);

	m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, channels, m_frames, true, MIPRaw16bitAudioMessage::Native, m_pMsgBuffer, true);
	
	if (!m_sigWait.init())
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}
	if (!m_sigWait2.init())
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		m_sigWait.destroy();
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}

	if (m_pRecorder->start() != 0)
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		m_sigWait.destroy();
		m_sigWait2.destroy();
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_CANTSTARTRECORDER);
		return false;
	}

	m_stopThread = false;
	if (JThread::Start() < 0)
	{
		delete m_pRecorder;
		m_pRecorder = 0;
		delete [] m_pLastInputCopy;
		delete m_pMsg;
		m_sigWait.destroy();
		m_sigWait2.destroy();
		for (int i = 0 ; i < m_numBuffers ; i++)
			delete m_pRecordBuffers[i];
		delete [] m_pRecordBuffers;
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_CANTSTARTTHREAD);
		return false;
	}
	
	return true;
}

bool MIPAudioRecorderInput::close()
{
	if (m_pRecorder == 0)
	{
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_NOTOPENED);
		return false;
	}

	m_pRecorder->stop();
	delete m_pRecorder;
	m_pRecorder = 0;
	
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

bool MIPAudioRecorderInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pRecorder == 0)
	{
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_NOTOPENED);
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

		memcpy(m_pMsgBuffer, m_pLastInputCopy, m_bufSize*sizeof(int16_t));
		
		m_gotLastInput = true;
		m_pMsg->setTime(m_sampleInstant);

		m_frameMutex.Unlock();
		
		m_gotMsg = false;
	}
	else
	{
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_BADMESSAGE);
		return false;
	}

	return true;
}

bool MIPAudioRecorderInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pRecorder == 0)
	{
		setErrorString(MIPAUDIORECORDERINPUT_ERRSTR_NOTOPENED);
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

void MIPAudioRecorderInput::StaticAudioRecorderCallback(int event, void *pUser, void *pInfo)
{
	if (event == android::AudioRecord::EVENT_MORE_DATA)
	{
		MIPAudioRecorderInput *pInstance = (MIPAudioRecorderInput *)pUser;

		pInstance->AudioRecorderCallback(pInfo);
	}
}

void MIPAudioRecorderInput::AudioRecorderCallback(void *pInfo)
{
	android::AudioRecord::Buffer *pBuffer = (android::AudioRecord::Buffer *)pInfo;

	// TODO: write some error here
	if (pBuffer->size != m_blockSize*sizeof(uint16_t)) 
		return;

	RecordBuffer *pBuf = m_pRecordBuffers[m_curBuffer];
	
	memcpy(pBuf->getBuffer(), pBuffer->i16, m_blockSize*sizeof(uint16_t));
	pBuf->setOffset(0);

	m_curBuffer++;
	if (m_curBuffer == m_numBuffers)
		m_curBuffer = 0;

	m_blockMutex.Lock();
	m_availableFrames += m_blockFrames;
	m_blockMutex.Unlock();

	m_sigWait2.signal();
}

void *MIPAudioRecorderInput::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPAudioRecorderInput::Thread started" << std::endl;
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

					uint16_t *pFrames = m_pRecordBuffers[m_curReadBuffer]->getBuffer();
					
					if (num > maxNum)
						num = maxNum;

					if (m_channels == 2)
					{
						for (j = 0, k = curOffset*2 ; j < num ; j++)
						{
							m_pLastInputCopy[i++] = pFrames[k++];
							m_pLastInputCopy[i++] = pFrames[k++];
						}
					}
					else // m_channels == 1
					{
						for (j = 0, k = curOffset ; j < num ; j++)
							m_pLastInputCopy[i++] = pFrames[k++];
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
	std::cout << "MIPAudioRecorderInput::Thread started" << std::endl;
#endif // MIPDEBUG
}

#endif // MIPCONFIG_SUPPORT_AUDIORECORDER

