/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2017  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_ALSA

#include "mipalsainput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include "mipstreambuffer.h"
#include <iostream>

#include "mipdebug.h"

using namespace jthread;
using namespace std;

#define MIPALSAINPUT_ERRSTR_DEVICEALREADYOPEN				"Device already opened"
#define MIPALSAINPUT_ERRSTR_DEVICENOTOPEN					"Device is not opened"
#define MIPALSAINPUT_ERRSTR_CANTOPENDEVICE					"Error opening device"
#define MIPALSAINPUT_ERRSTR_CANTGETHWPARAMS					"Error retrieving device hardware parameters"
#define MIPALSAINPUT_ERRSTR_CANTSETINTERLEAVEDMODE			"Can't set device to interleaved mode"
#define MIPALSAINPUT_ERRSTR_FLOATNOTSUPPORTED				"Floating point format is not supported by this device"
#define MIPALSAINPUT_ERRSTR_S16NOTSUPPORTED					"Signed 16 bit format is not supported by this device"
#define MIPALSAINPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE			"The requested sampling rate is not supported"
#define MIPALSAINPUT_ERRSTR_UNSUPPORTEDCHANNELS				"The requested number of channels is not supported"
#define MIPALSAINPUT_ERRSTR_CANTSETHWPARAMS					"Error setting hardware parameters"
#define MIPALSAINPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD		"Can't start the background thread"
#define MIPALSAINPUT_ERRSTR_THREADSTOPPED					"Background thread stopped"
#define MIPALSAINPUT_ERRSTR_INCOMPATIBLECHANNELS			"Incompatible number of channels"
#define MIPALSAINPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE		"Incompatible sampling rate"
#define MIPALSAINPUT_ERRSTR_CANTINITSIGWAIT					"Unable to initialize signal waiter"
#define MIPALSAINPUT_ERRSTR_BADMESSAGE						"Bad message, expecting a WAITTIME message"

MIPAlsaInput::MIPAlsaInput() : MIPComponent("MIPAlsaInput")
{
	int status;
	
	m_pDevice = 0;
	
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize ALSA output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}

	m_pBuffer = nullptr;
	m_pMsg = nullptr;
}

MIPAlsaInput::~MIPAlsaInput()
{
	close();
}

bool MIPAlsaInput::open(int sampRate, int channels, MIPTime blockTime, bool floatSamples, const string device)
{
	if (m_pDevice != 0)
	{
		setErrorString(MIPALSAINPUT_ERRSTR_DEVICEALREADYOPEN);
		return false;
	}

	if (!m_sigWait.isInit())
	{
		if (!m_sigWait.init())
		{
			setErrorString(MIPALSAINPUT_ERRSTR_CANTINITSIGWAIT);
			return false;
		}
		m_sigWait.clearSignalBuffers();
	}
	
	int status;

	m_floatSamples = floatSamples;
	
	if ((status = snd_pcm_open(&m_pDevice, device.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0)
	{
		m_pDevice = 0;
		setErrorString(MIPALSAINPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}
	
	snd_pcm_hw_params_malloc(&m_pHwParameters);

	if ((status = snd_pcm_hw_params_any(m_pDevice,m_pHwParameters)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAINPUT_ERRSTR_CANTGETHWPARAMS);
		return false;
	}

	if ((status = snd_pcm_hw_params_set_access(m_pDevice, m_pHwParameters, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAINPUT_ERRSTR_CANTSETINTERLEAVEDMODE);
		return false;
	}

	if (floatSamples)
	{
		if ((status = snd_pcm_hw_params_set_format(m_pDevice, m_pHwParameters, SND_PCM_FORMAT_FLOAT)) < 0)
		{
			snd_pcm_hw_params_free(m_pHwParameters);
			snd_pcm_close(m_pDevice);
			m_pDevice = 0;
			setErrorString(MIPALSAINPUT_ERRSTR_FLOATNOTSUPPORTED);
			return false;
		}
	}
	else
	{
		if ((status = snd_pcm_hw_params_set_format(m_pDevice, m_pHwParameters, SND_PCM_FORMAT_S16)) < 0)
		{
			snd_pcm_hw_params_free(m_pHwParameters);
			snd_pcm_close(m_pDevice);
			m_pDevice = 0;
			setErrorString(MIPALSAINPUT_ERRSTR_S16NOTSUPPORTED);
			return false;
		}
	}

	// For some reason this does not work well on my soundcard at home
	//if ((status = snd_pcm_hw_params_set_rate(m_pDevice, m_pHwParameters, sampRate, 0)) < 0)

	unsigned int rate = sampRate;
	if ((status = snd_pcm_hw_params_set_rate_near(m_pDevice, m_pHwParameters, &rate, 0)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAINPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE);
		return false;
	}

	if (rate != (unsigned int)sampRate)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAINPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE);
		return false;
	}

	if ((status = snd_pcm_hw_params_set_channels(m_pDevice, m_pHwParameters, channels)) < 0)
	{	
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAINPUT_ERRSTR_UNSUPPORTEDCHANNELS);
		return false;
	}

	m_blockFrames = ((size_t)((blockTime.getValue() * ((real_t)sampRate)) + 0.5));
	m_blockSize = m_blockFrames * ((size_t)channels);

	if (floatSamples)
		m_blockSize *= sizeof(float);
	else 
		m_blockSize *= sizeof(int16_t);

	snd_pcm_uframes_t minVal, maxVal;
	
	minVal = m_blockFrames;
	maxVal = m_blockFrames*10;

	snd_pcm_hw_params_set_buffer_size_minmax(m_pDevice, m_pHwParameters, &minVal, &maxVal);
	//cout << "minVal = " << minVal << " maxVal = " << maxVal << endl;
	
	if ((status = snd_pcm_hw_params(m_pDevice, m_pHwParameters)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(std::string(MIPALSAINPUT_ERRSTR_CANTSETHWPARAMS)+std::string(": ")+std::string(snd_strerror(status)));
		return false;
	}


	m_pBuffer = new MIPStreamBuffer(m_blockSize);

	m_channels = channels;
	m_sampRate = sampRate;
	
	if (floatSamples)
	{
		m_floatFrames.resize(m_blockFrames*m_channels*10);
		m_pMsg = new MIPRawFloatAudioMessage(sampRate, channels, m_blockFrames, &m_floatFrames[0], false);
	}
	else
	{
		m_s16Frames.resize(m_blockFrames*m_channels*10);
		m_pMsg = new MIPRaw16bitAudioMessage(sampRate, channels, m_blockFrames, true, MIPRaw16bitAudioMessage::Native,
				                             &m_s16Frames[0], false);
	}

	m_stopLoop = false;
	if ((status = JThread::Start()) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		delete m_pBuffer;
		m_pBuffer = nullptr;
		delete m_pMsg;
		m_pMsg = nullptr;

		setErrorString(MIPALSAINPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD);
		return false;
	}
	
	return true;
}

bool MIPAlsaInput::close()
{
	if (m_pDevice == 0)
	{
		setErrorString(MIPALSAINPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	MIPTime starttime = MIPTime::getCurrentTime();
	
	m_stopMutex.Lock();
	m_stopLoop = true;
	m_stopMutex.Unlock();
	
	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue()-starttime.getValue()) < 5.0)
	{
		MIPTime::wait(MIPTime(0.010));
		m_sigWait.signal();
	}

	if (JThread::IsRunning())
		JThread::Kill();

	snd_pcm_hw_params_free(m_pHwParameters);
	snd_pcm_reset(m_pDevice);
	snd_pcm_close(m_pDevice);
	m_pDevice = 0;

	m_sigWait.clearSignalBuffers();
	delete m_pBuffer;
	m_pBuffer = nullptr;
	delete m_pMsg;
	m_pMsg = nullptr;

	return true;
}

bool MIPAlsaInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pDevice == 0)
	{
		setErrorString(MIPALSAINPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}
	
	if (!JThread::IsRunning())
	{
		setErrorString(MIPALSAINPUT_ERRSTR_THREADSTOPPED);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME))
	{
		setErrorString(MIPALSAINPUT_ERRSTR_BADMESSAGE);
		return false;
	}

	m_sigWait.clearSignalBuffers();
	
	if (m_pBuffer->getAmountBuffered() < m_blockSize)
		m_sigWait.waitForSignal();

	void *pTgt = (m_floatSamples)?((void *)&m_floatFrames[0]):((void *)&m_s16Frames[0]);
	m_pBuffer->read(pTgt, m_blockSize);

	m_gotMsg = false;
	
	return true;
}

bool MIPAlsaInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pDevice == 0)
	{
		setErrorString(MIPALSAINPUT_ERRSTR_DEVICENOTOPEN);
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

void *MIPAlsaInput::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPAlsaInput::Thread started" << std::endl;
#endif // MIPDEBUG

	JThread::ThreadStarted();

	vector<uint8_t> block(m_blockSize);

	bool done;
	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();

	while (!done)
	{
		int r = 0;
		MIPTime t = MIPTime::getCurrentTime();
		if ((r = snd_pcm_readi(m_pDevice, &block[0], m_blockFrames)) >= 0)
		{
			MIPTime dt = MIPTime::getCurrentTime();
			dt -= t;
			
			if (m_pBuffer->getAmountBuffered() > m_blockSize) // try not to build up delay
				m_pBuffer->clear();

			m_pBuffer->write(&block[0], m_blockSize);
			m_sigWait.signal();
		}
		else
		{
			cerr << "Error reading data, trying to recover: " << snd_strerror(r) << endl;
			snd_pcm_recover(m_pDevice, r, 0);
		}

		m_stopMutex.Lock();
		done = m_stopLoop;
		m_stopMutex.Unlock();
	}

#ifdef MIPDEBUG
	std::cout << "MIPAlsaInput::Thread stopped" << std::endl;
#endif // MIPDEBUG
	return 0;
}

#endif // MIPCONFIG_SUPPORT_ALSA

