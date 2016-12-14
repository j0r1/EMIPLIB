/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2011  Hasselt University - Expertise Centre for
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

#include "mipwinmmoutput.h"
#include "miprawaudiomessage.h"
#include <iostream>

#include "mipdebug.h"

#define MIPWINMMOUTPUT_ERRSTR_DEVICEALREADYOPEN		"Device already opened"
#define MIPWINMMOUTPUT_ERRSTR_DEVICENOTOPEN		"Device is not opened"
#define MIPWINMMOUTPUT_ERRSTR_CANTOPENDEVICE		"Error opening device"
#define MIPWINMMOUTPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD	"Can't start the background thread"
#define MIPWINMMOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"No pull available for this component"
#define MIPWINMMOUTPUT_ERRSTR_THREADSTOPPED		"Background thread stopped"
#define MIPWINMMOUTPUT_ERRSTR_BADMESSAGE			"Only raw audio messages are supported"
#define MIPWINMMOUTPUT_ERRSTR_INCOMPATIBLECHANNELS	"Incompatible number of channels"
#define MIPWINMMOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPWINMMOUTPUT_ERRSTR_INCOMPATIBLEFRAMES		"Incompatible number of frames"
#define MIPWINMMOUTPUT_ERRSTR_BUFFERFULL			"Buffer full"
#define MIPWINMMOUTPUT_ERRSTR_CANTRESETDEVICE		"Can't reset the device"
#define MIPWINMMOUTPUT_ERRSTR_CANTUNPREPAREHEADER	"Can't unprepare header"
#define MIPWINMMOUTPUT_ERRSTR_CANTCLOSEDEVICE		"Can't close the device"
#define MIPWINMMOUTPUT_ERRSTR_CANTPREPAREHEADER		"Can't prepare header"
#define MIPWINMMOUTPUT_ERRSTR_CANTWRITEDATA			"Can't write data"

MIPWinMMOutput::MIPWinMMOutput() : MIPComponent("MIPWinMMOutput"), m_prevCheckTime(0), m_delay(0)
{
	m_init = false;

	int status;
	
	if ((status = m_bufCountMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize WinMM output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPWinMMOutput::~MIPWinMMOutput()
{
	close();
}

bool MIPWinMMOutput::open(int sampRate, int channels, MIPTime blockTime, MIPTime bufferTime, bool highPriority, UINT deviceID)
{
	if (m_init)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_DEVICEALREADYOPEN);
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
	
	if (waveOutOpen((LPHWAVEOUT)&m_device, deviceID, (LPWAVEFORMATEX)&format,(DWORD_PTR)outputCallback,(DWORD_PTR)this,CALLBACK_FUNCTION))
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}
	
	m_blockFrames = ((size_t)((blockTime.getValue() * ((real_t)sampRate)) + 0.5));
	m_blockLength = m_blockFrames * ((size_t)channels);

	m_numBlocks = (int)((bufferTime.getValue() / blockTime.getValue())+0.5);
	if (m_numBlocks < 10)
		m_numBlocks = 10;

	m_frameArray = new WAVEHDR[m_numBlocks];

	for (int i = 0 ; i < m_numBlocks ; i++)
	{
		m_frameArray[i].dwBufferLength = (DWORD)(m_blockLength*sizeof(uint16_t));
		m_frameArray[i].lpData = (LPSTR) new uint16_t [m_blockLength];
	}

	m_sampRate = sampRate;
	m_channels = channels;

	m_blockPos = 0;
	m_blocksInitialized = 0;
	m_bufCount = 0;
	m_flushBuffers = false;
	m_prevBufCount = 0;
	m_prevCheckTime = MIPTime(0);

	if (highPriority)
		m_threadPrioritySet = false;
	else
		m_threadPrioritySet = true;
	m_init = true;
	
	return true;
}

bool MIPWinMMOutput::close()
{
	if (!m_init)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	if (waveOutReset(m_device) != MMSYSERR_NOERROR)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_CANTRESETDEVICE);
		return false;
	}
	for (int i = 0 ; i < m_blocksInitialized ; i++)
	{
		if (waveOutUnprepareHeader(m_device,&m_frameArray[i],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			setErrorString(MIPWINMMOUTPUT_ERRSTR_CANTUNPREPAREHEADER);
			return false;
		}
	}
	for (int i = 0 ; i < m_numBlocks ; i++)
	{
		uint16_t *pBuf = (uint16_t *)m_frameArray[i].lpData;
		delete [] pBuf;
	}	

	if (waveOutClose(m_device) != MMSYSERR_NOERROR)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_CANTCLOSEDEVICE);
		return false;
	}
	
	delete [] m_frameArray;

	m_init = false;
		
	return true;
}

bool MIPWinMMOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16LE))
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (!m_init)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}
	
	if (!m_threadPrioritySet)
	{
		m_threadPrioritySet = true;
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	}
	
	MIPRaw16bitAudioMessage *audioMessage = (MIPRaw16bitAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}
	if (audioMessage->getNumberOfFrames() != m_blockFrames)
	{
		setErrorString(MIPWINMMOUTPUT_ERRSTR_INCOMPATIBLEFRAMES);
		return false;
	}

	int bufCount;
	int loops = 1;

	m_bufCountMutex.Lock();
	bufCount = m_bufCount;
	m_bufCountMutex.Unlock();
//	std::cout << bufCount << std::endl;

	if (m_prevCheckTime.getSeconds() == 0)
		m_prevCheckTime = MIPTime::getCurrentTime();

	if (m_flushBuffers) // wait a while
	{
		if (bufCount > m_prevBufCount)
			return true;
		m_flushBuffers = false;
	}
	else
	{
		if ((MIPTime::getCurrentTime().getValue() - m_prevCheckTime.getValue()) > 1.0) // wait at least a second
		{
			if (bufCount - m_prevBufCount > 2) // getting out of sync: delay buildup
			{
//				std::cout << "HERE" << std::endl;
				m_flushBuffers = true;
				return true;
			}
			else if (bufCount - m_prevBufCount <= 0)
			{
				loops = 2;
			}
		}
		else
			m_prevBufCount = bufCount;
	}

	size_t num = audioMessage->getNumberOfFrames() * m_channels;
	const uint16_t *pFrames = audioMessage->getFrames();

	for (int i = 0 ; i < loops ; i++)
	{
		// add the actual frame, duplicating it if we're running low on buffers

		if (m_blocksInitialized == m_numBlocks)
		{
			if (waveOutUnprepareHeader(m_device,&m_frameArray[m_blockPos],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			{
				setErrorString(MIPWINMMOUTPUT_ERRSTR_CANTUNPREPAREHEADER);
				return false;
			}
		}

		uint16_t *pDst = (uint16_t *)m_frameArray[m_blockPos].lpData;

		memcpy(pDst, pFrames, num*sizeof(uint16_t));

		m_frameArray[m_blockPos].dwFlags = 0;
		m_frameArray[m_blockPos].dwLoops = 0;
		m_frameArray[m_blockPos].dwUser = 0;
		m_bufCountMutex.Lock();
		m_bufCount++;
		m_bufCountMutex.Unlock();

		if (waveOutPrepareHeader(m_device,&m_frameArray[m_blockPos],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			setErrorString(MIPWINMMOUTPUT_ERRSTR_CANTPREPAREHEADER);
			return false;
		}
		if (waveOutWrite(m_device,&m_frameArray[m_blockPos],sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			setErrorString(MIPWINMMOUTPUT_ERRSTR_CANTWRITEDATA);
			return false;
		}

		m_blockPos++;
		if (m_blockPos == m_numBlocks)
			m_blockPos = 0;
		if (m_blocksInitialized != m_numBlocks)
			m_blocksInitialized++;

	}

	return true;
}

bool MIPWinMMOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPWINMMOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return true;
}

void CALLBACK MIPWinMMOutput::outputCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg == WOM_DONE)
	{
		MIPWinMMOutput *pOutput = (MIPWinMMOutput *)dwInstance;

		pOutput->m_bufCountMutex.Lock();
		pOutput->m_bufCount--;
		pOutput->m_bufCountMutex.Unlock();
	}
}

#endif // MIPCONFIG_SUPPORT_WINMM

