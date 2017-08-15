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

#include "mipalsaoutput.h"
#include "miprawaudiomessage.h"
#include "mipstreambuffer.h"
#include <iostream>

#include "mipdebug.h"

using namespace jthread;
using namespace std;

#define MIPALSAOUTPUT_ERRSTR_DEVICEALREADYOPEN			"Device already opened"
#define MIPALSAOUTPUT_ERRSTR_DEVICENOTOPEN				"Device is not opened"
#define MIPALSAOUTPUT_ERRSTR_CANTOPENDEVICE				"Error opening device"
#define MIPALSAOUTPUT_ERRSTR_CANTGETHWPARAMS			"Error retrieving device hardware parameters"
#define MIPALSAOUTPUT_ERRSTR_CANTSETINTERLEAVEDMODE		"Can't set device to interleaved mode"
#define MIPALSAOUTPUT_ERRSTR_FLOATNOTSUPPORTED			"Floating point format is not supported by this device"
#define MIPALSAOUTPUT_ERRSTR_S16NOTSUPPORTED			"Signed 16 bit format is not supported by this device"
#define MIPALSAOUTPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE	"The requested sampling rate is not supported"
#define MIPALSAOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS		"The requested number of channels is not supported"
#define MIPALSAOUTPUT_ERRSTR_CANTSETHWPARAMS			"Error setting hardware parameters"
#define MIPALSAOUTPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD	"Can't start the background thread"
#define MIPALSAOUTPUT_ERRSTR_PULLNOTIMPLEMENTED			"No pull available for this component"
#define MIPALSAOUTPUT_ERRSTR_THREADSTOPPED				"Background thread stopped"
#define MIPALSAOUTPUT_ERRSTR_BADMESSAGE					"Only raw audio messages are supported"
#define MIPALSAOUTPUT_ERRSTR_INCOMPATIBLECHANNELS		"Incompatible number of channels"
#define MIPALSAOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPALSAOUTPUT_ERRSTR_INCOMPATIBLEBLOCKSIZE		"Incompatible number of frames in message"
#define MIPALSAOUTPUT_ERRSTR_CANTINITSIGWAIT			"Unable to initialize signal waiter"

MIPAlsaOutput::MIPAlsaOutput() : MIPComponent("MIPAlsaOutput")
{
	int status;
	
	m_pDevice = 0;
	
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize ALSA output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}

	m_pBuffer = nullptr;
}

MIPAlsaOutput::~MIPAlsaOutput()
{
	close();
}

bool MIPAlsaOutput::open(int sampRate, int channels, MIPTime blockTime, bool floatSamples, const string device)
{
	if (m_pDevice != 0)
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_DEVICEALREADYOPEN);
		return false;
	}

	if (!m_sigWait.isInit())
	{
		if (!m_sigWait.init())
		{
			setErrorString(MIPALSAOUTPUT_ERRSTR_CANTINITSIGWAIT);
			return false;
		}
		m_sigWait.clearSignalBuffers();
	}
	
	int status;

	m_floatSamples = floatSamples;
	
	if ((status = snd_pcm_open(&m_pDevice, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
	{
		m_pDevice = 0;
		setErrorString(MIPALSAOUTPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}
	
	snd_pcm_hw_params_malloc(&m_pHwParameters);

	if ((status = snd_pcm_hw_params_any(m_pDevice,m_pHwParameters)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAOUTPUT_ERRSTR_CANTGETHWPARAMS);
		return false;
	}

	if ((status = snd_pcm_hw_params_set_access(m_pDevice, m_pHwParameters, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAOUTPUT_ERRSTR_CANTSETINTERLEAVEDMODE);
		return false;
	}

	if (floatSamples)
	{
		if ((status = snd_pcm_hw_params_set_format(m_pDevice, m_pHwParameters, SND_PCM_FORMAT_FLOAT)) < 0)
		{
			snd_pcm_hw_params_free(m_pHwParameters);
			snd_pcm_close(m_pDevice);
			m_pDevice = 0;
			setErrorString(MIPALSAOUTPUT_ERRSTR_FLOATNOTSUPPORTED);
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
			setErrorString(MIPALSAOUTPUT_ERRSTR_S16NOTSUPPORTED);
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
		setErrorString(MIPALSAOUTPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE);
		return false;
	}

	if (rate != (unsigned int)sampRate)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAOUTPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE);
		return false;
	}

	if ((status = snd_pcm_hw_params_set_channels(m_pDevice, m_pHwParameters, channels)) < 0)
	{	
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS);
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
	maxVal = m_blockFrames*2;

	snd_pcm_hw_params_set_buffer_size_minmax(m_pDevice, m_pHwParameters, &minVal, &maxVal);
	//cout << "minVal = " << minVal << " maxVal = " << maxVal << endl;
	
	if ((status = snd_pcm_hw_params(m_pDevice, m_pHwParameters)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(std::string(MIPALSAOUTPUT_ERRSTR_CANTSETHWPARAMS)+std::string(": ")+std::string(snd_strerror(status)));
		return false;
	}

	m_pBuffer = new MIPStreamBuffer(m_blockSize);

	m_channels = channels;
	m_sampRate = sampRate;
	
	m_stopLoop = false;
	if ((status = JThread::Start()) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		delete m_pBuffer;
		m_pBuffer = nullptr;
		setErrorString(MIPALSAOUTPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD);
		return false;
	}
	
	return true;
}

bool MIPAlsaOutput::close()
{
	if (m_pDevice == 0)
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_DEVICENOTOPEN);
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

	return true;
}

bool MIPAlsaOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
		((pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT && m_floatSamples) ||
		 (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 && !m_floatSamples) ) ) )
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pDevice == 0)
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}
	
	if (!JThread::IsRunning())
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_THREADSTOPPED);
		return false;
	}

	MIPAudioMessage *audioMessage = (MIPAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}
	
	size_t num = audioMessage->getNumberOfFrames();
	if (num != m_blockFrames)
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_INCOMPATIBLEBLOCKSIZE);
		return false;
	}

	assert(m_pBuffer);

	if (m_floatSamples)
	{
		MIPRawFloatAudioMessage *audioMessageFloat = (MIPRawFloatAudioMessage *)pMsg;
		const float *frames = audioMessageFloat->getFrames();

		m_pBuffer->write(frames, m_blockSize);
	}
	else // integer samples
	{
		MIPRaw16bitAudioMessage *audioMessageInt = (MIPRaw16bitAudioMessage *)pMsg;
		const uint16_t *frames = audioMessageInt->getFrames();
	
		m_pBuffer->write(frames, m_blockSize);
	}
	m_sigWait.signal();
	
	return true;
}

bool MIPAlsaOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPALSAOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return false;
}

void *MIPAlsaOutput::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPAlsaOutput::Thread started" << std::endl;
#endif // MIPDEBUG

	JThread::ThreadStarted();

	// As a small buffer, empty block of 0.020 sec
	int emptyBlockSize = (int)(0.020*m_sampRate+0.5) * m_channels;
	if (m_floatSamples)
		emptyBlockSize *= sizeof(float);
	else
		emptyBlockSize *= sizeof(int16_t);

	vector<uint8_t> emptyBlock(emptyBlockSize, 0);
	vector<uint8_t> block(m_blockSize);

	bool done;
	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();

	bool justGotUnderrun = true; // insert a small block as a buffer

	snd_pcm_sw_params_t *pSwParams;
	snd_pcm_sw_params_malloc(&pSwParams);

	while (!done)
	{
		m_sigWait.waitForSignal();

		if (m_pBuffer->getAmountBuffered() >= m_blockSize)
		{
			if (justGotUnderrun)
			{
				justGotUnderrun = false;
				//cerr << "Trying to protect against underrun by writing empty block first" << endl;
				for (int i = 0 ; i < 1 ; i++)
					snd_pcm_writei(m_pDevice, &emptyBlock[0], emptyBlockSize);
			}

			int r;
			m_pBuffer->read(&block[0], m_blockSize);
			if ((r = snd_pcm_writei(m_pDevice, &block[0], m_blockFrames)) < 0)
			{
				//cerr << "Trying to recover from " << r  << endl;
				snd_pcm_recover(m_pDevice, r, 0);
				if (r == -EPIPE)
					justGotUnderrun = true;
			}
		}
		
		//cerr << m_pBuffer->getAmountBuffered() << endl;
		if (m_pBuffer->getAmountBuffered() > m_blockSize*2)
		{
			//cerr << "Clearing buffer, seems to build up audio" << endl;
			m_pBuffer->clear();
		}

		//snd_pcm_sframes_t avail, delay;
		//snd_pcm_avail_delay(m_pDevice, &avail, &delay);
		//cerr << avail << " " << delay << " " << avail+delay << " " <<  m_blockFrames << endl;

		m_stopMutex.Lock();
		done = m_stopLoop;
		m_stopMutex.Unlock();
	}
	snd_pcm_sw_params_free(pSwParams);

#ifdef MIPDEBUG
	std::cout << "MIPAlsaOutput::Thread stopped" << std::endl;
#endif // MIPDEBUG
	return 0;
}

#endif // MIPCONFIG_SUPPORT_ALSA

