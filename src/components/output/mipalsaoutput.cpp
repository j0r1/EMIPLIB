/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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
#include <iostream>

#define MIPALSAOUTPUT_ERRSTR_DEVICEALREADYOPEN		"Device already opened"
#define MIPALSAOUTPUT_ERRSTR_DEVICENOTOPEN		"Device is not opened"
#define MIPALSAOUTPUT_ERRSTR_CANTOPENDEVICE		"Error opening device"
#define MIPALSAOUTPUT_ERRSTR_CANTGETHWPARAMS		"Error retrieving device hardware parameters"
#define MIPALSAOUTPUT_ERRSTR_CANTSETINTERLEAVEDMODE	"Can't set device to interleaved mode"
#define MIPALSAOUTPUT_ERRSTR_FLOATNOTSUPPORTED		"Floating point format is not supported by this device"
#define MIPALSAOUTPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE	"The requested sampling rate is not supported"
#define MIPALSAOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS	"The requested number of channels is not supported"
#define MIPALSAOUTPUT_ERRSTR_CANTSETHWPARAMS		"Error setting hardware parameters"
#define MIPALSAOUTPUT_ERRSTR_BLOCKTIMETOOLARGE		"Block time too large"
#define MIPALSAOUTPUT_ERRSTR_CANTSTARTBACKGROUNDTHREAD	"Can't start the background thread"
#define MIPALSAOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"No pull available for this component"
#define MIPALSAOUTPUT_ERRSTR_THREADSTOPPED		"Background thread stopped"
#define MIPALSAOUTPUT_ERRSTR_BADMESSAGE			"Only raw audio messages are supported"
#define MIPALSAOUTPUT_ERRSTR_INCOMPATIBLECHANNELS	"Incompatible number of channels"
#define MIPALSAOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPALSAOUTPUT_ERRSTR_BUFFERFULL			"Buffer full"

MIPAlsaOutput::MIPAlsaOutput() : MIPComponent("MIPAlsaOutput"), m_delay(0), m_distTime(0), m_blockTime(0)
{
	int status;
	
	m_pDevice = 0;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize ALSA output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize ALSA output thread mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPAlsaOutput::~MIPAlsaOutput()
{
	close();
}

bool MIPAlsaOutput::open(int sampRate, int channels, const std::string device, 
		  MIPTime blockTime, MIPTime arrayTime, bool floatSamples)
{
	if (m_pDevice != 0)
	{
		setErrorString(MIPALSAOUTPUT_ERRSTR_DEVICEALREADYOPEN);
		return false;
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
			setErrorString(MIPALSAOUTPUT_ERRSTR_FLOATNOTSUPPORTED);
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

	snd_pcm_uframes_t val;
	unsigned int per;
	int dir;
	
	snd_pcm_hw_params_set_buffer_size_min(m_pDevice, m_pHwParameters, &val);
	snd_pcm_hw_params_set_periods_min(m_pDevice, m_pHwParameters, &per, &dir);
	
	if ((status = snd_pcm_hw_params(m_pDevice, m_pHwParameters)) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(std::string(MIPALSAOUTPUT_ERRSTR_CANTSETHWPARAMS)+std::string(": ")+std::string(snd_strerror(status)));
		return false;
	}

	m_frameArrayLength = ((size_t)((arrayTime.getValue() * ((real_t)sampRate)) + 0.5)) * ((size_t)channels);
	m_blockFrames = ((size_t)((blockTime.getValue() * ((real_t)sampRate)) + 0.5));
	m_blockLength = m_blockFrames * ((size_t)channels);

	if (m_blockLength > m_frameArrayLength/4)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		setErrorString(MIPALSAOUTPUT_ERRSTR_BLOCKTIMETOOLARGE);
		return false;
	}

	// make sure a fixed number of blocks fit in the frame array
	size_t numBlocks = m_frameArrayLength / m_blockLength + 1;
	
	m_frameArrayLength = numBlocks * m_blockLength;

	if (floatSamples)
	{
		m_pFrameArrayFloat = new float[m_frameArrayLength];
		for (size_t i = 0 ; i < m_frameArrayLength ; i++)
			m_pFrameArrayFloat[i] = 0;
		m_pFrameArrayInt = 0;
	}
	else
	{
		m_pFrameArrayInt = new uint16_t[m_frameArrayLength];
		for (size_t i = 0 ; i < m_frameArrayLength ; i++)
			m_pFrameArrayInt[i] = 0;
		m_pFrameArrayFloat = 0;
	}
	
	m_currentPos = 0;
	m_nextPos = m_blockLength;
	m_channels = channels;
	m_sampRate = sampRate;
	m_distTime = blockTime;
	m_blockTime = blockTime;

	//std::cerr << "m_frameArrayLength: " << m_frameArrayLength << std::endl; 
	
	m_stopLoop = false;
	if ((status = JThread::Start()) < 0)
	{
		snd_pcm_hw_params_free(m_pHwParameters);
		snd_pcm_close(m_pDevice);
		m_pDevice = 0;
		if (m_pFrameArrayFloat)
			delete [] m_pFrameArrayFloat;
		if (m_pFrameArrayInt)
			delete [] m_pFrameArrayInt;
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
	}

	if (JThread::IsRunning())
		JThread::Kill();

	snd_pcm_hw_params_free(m_pHwParameters);
	snd_pcm_close(m_pDevice);
	m_pDevice = 0;

	if (m_pFrameArrayFloat)
		delete [] m_pFrameArrayFloat;
	if (m_pFrameArrayInt)
		delete [] m_pFrameArrayInt;

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
	
	size_t num = audioMessage->getNumberOfFrames() * m_channels;
	size_t offset = 0;

	if (m_floatSamples)
	{
		MIPRawFloatAudioMessage *audioMessageFloat = (MIPRawFloatAudioMessage *)pMsg;
		const float *frames = audioMessageFloat->getFrames();
	
		m_frameMutex.Lock();
		while (num > 0)
		{
			size_t maxAmount = m_frameArrayLength - m_nextPos;
			size_t amount = (num > maxAmount)?maxAmount:num;
			
			if (m_nextPos <= m_currentPos && m_nextPos + amount > m_currentPos)
			{
				//std::cerr << "Strange error" << std::endl;
				m_nextPos = m_currentPos + m_blockLength;
			}
			
			if (amount != 0)
				memcpy(m_pFrameArrayFloat + m_nextPos, frames + offset, amount*sizeof(float));
	
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
	}
	else // integer samples
	{
		MIPRaw16bitAudioMessage *audioMessageInt = (MIPRaw16bitAudioMessage *)pMsg;
		const uint16_t *frames = audioMessageInt->getFrames();
	
		m_frameMutex.Lock();
		while (num > 0)
		{
			size_t maxAmount = m_frameArrayLength - m_nextPos;
			size_t amount = (num > maxAmount)?maxAmount:num;
			
			if (m_nextPos <= m_currentPos && m_nextPos + amount > m_currentPos)
			{
				//std::cerr << "Strange error" << std::endl;
				m_nextPos = m_currentPos + m_blockLength;
			}
			
			if (amount != 0)
				memcpy(m_pFrameArrayInt + m_nextPos, frames + offset, amount*sizeof(uint16_t));
	
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
	}
	m_distTime += MIPTime(((real_t)audioMessage->getNumberOfFrames())/((real_t)m_sampRate));
	m_frameMutex.Unlock();
	
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

	bool done;
	
	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();

	while (!done)
	{
		if (m_floatSamples)
		{
			if (snd_pcm_writei(m_pDevice, m_pFrameArrayFloat + m_currentPos, m_blockFrames) < 0)
			{
				snd_pcm_prepare(m_pDevice);
				//std::cerr << "Preparing device" << std::endl;
			}
		}
		else
		{
			if (snd_pcm_writei(m_pDevice, m_pFrameArrayInt + m_currentPos, m_blockFrames) < 0)
			{
				snd_pcm_prepare(m_pDevice);
				//std::cerr << "Preparing device" << std::endl;
			}
		}
		
		m_frameMutex.Lock();
		m_currentPos += m_blockLength;
		m_distTime -= m_blockTime;
		if (m_currentPos + m_blockLength > m_frameArrayLength)
		{
			m_currentPos = 0;
			//std::cerr << "Cycling" << std::endl;
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
			//std::cerr << "Adjusting to runaway input (" << m_distTime.getString() << ")" << std::endl;
		}
		m_frameMutex.Unlock();
		
		m_stopMutex.Lock();
		done = m_stopLoop;
		m_stopMutex.Unlock();
	}
#ifdef MIPDEBUG
	std::cout << "MIPAlsaOutput::Thread stopped" << std::endl;
#endif // MIPDEBUG
	return 0;
}

#endif // MIPCONFIG_SUPPORT_ALSA

