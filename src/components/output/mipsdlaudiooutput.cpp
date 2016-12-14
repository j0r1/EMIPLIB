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

#ifdef MIPCONFIG_SUPPORT_SDLAUDIO

#include "mipsdlaudiooutput.h"
#include "miprawaudiomessage.h"
#include <SDL/SDL_audio.h>
#include <SDL/SDL_error.h>
#include <iostream>

#include "mipdebug.h"

#define MIPSDLAUDIOOUTPUT_ERRSTR_ALREADYOPEN			"SDL audio component is already initialized"
#define MIPSDLAUDIOOUTPUT_ERRSTR_ARRAYTIMETOOSMALL		"Array time too small"
#define MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTOPEN			"Couldn't open SDL audio device: "
#define MIPSDLAUDIOOUTPUT_ERRSTR_NOTOPENED			"SDL audio component is not initialized"
#define MIPSDLAUDIOOUTPUT_ERRSTR_BADMESSAGE			"Bad message"
#define MIPSDLAUDIOOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPSDLAUDIOOUTPUT_ERRSTR_INCOMPATIBLECHANNELS		"Incompatible number of channels"
#define MIPSDLAUDIOOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"Pull not implemented"
#define MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTGETFORMAT		"Unable to open sound device for 16 bit native endian audio"
#define MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTGETCHANNELS		"Unable to open sound device for requested number of channels"
#define MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTGETSAMPLINGRATE		"Unable to open sound device at the specified sampling rate"

MIPSDLAudioOutput::MIPSDLAudioOutput() : MIPComponent("MIPSDLAudioOutput"), m_delay(0), m_distTime(0), m_blockTime(0), m_interval(0)
{
	int status;
	
	m_init = false;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize Jack callback mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPSDLAudioOutput::~MIPSDLAudioOutput()
{
	close();
}


bool MIPSDLAudioOutput::open(int sampRate, int channels, MIPTime interval, MIPTime arrayTime)
{
	if (m_init != 0)
	{
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_ALREADYOPEN);
		return false;
	}
	
	int status;

	SDL_AudioSpec spec, rcvSpec;

	spec.freq = sampRate;
	spec.channels = channels;
	spec.format = AUDIO_S16SYS;
	spec.callback = StaticCallback;
	spec.userdata = this;

	size_t l = (size_t)((interval.getValue() * (real_t)sampRate)+0.5);
	m_blockLength = 1;
	while (m_blockLength < l && m_blockLength < 65536*1024)
		m_blockLength <<= 1;

	m_blockLength >>= 1;
	if (m_blockLength == 0)
		m_blockLength = 1;

	spec.samples = m_blockLength*channels;

	if (SDL_OpenAudio(&spec, &rcvSpec) < 0)
	{
		setErrorString(std::string(MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTOPEN) + std::string(SDL_GetError()));
		return false;
	}

	if (rcvSpec.freq != sampRate)
	{
		SDL_CloseAudio();
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTGETSAMPLINGRATE);
		return false;
	}
	
	if (rcvSpec.channels != channels)
	{
		SDL_CloseAudio();
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTGETCHANNELS);
		return false;
	}

	if (rcvSpec.format != AUDIO_S16SYS)
	{
		SDL_CloseAudio();
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_COULDNTGETFORMAT);
		return false;
	}

	m_blockLength = rcvSpec.samples; // apparently, the value returned is actually the number of frames
	
	m_sampRate = sampRate;
	m_channels = channels;
	m_blockTime = MIPTime((real_t)m_blockLength/(real_t)m_sampRate);
	m_frameArrayLength = ((size_t)((arrayTime.getValue() * ((real_t)m_sampRate)) + 0.5));
	m_intervalLength = ((((size_t)((interval.getValue()*(real_t)m_sampRate)+0.5))/m_blockLength)+1)*m_blockLength;
	m_interval = MIPTime((real_t)m_intervalLength/(real_t)m_sampRate);

	if (m_intervalLength > m_frameArrayLength/4)
	{
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_ARRAYTIMETOOSMALL);
		return false;
	}

	// make sure a fixed number of blocks fit in the frame array
	size_t numBlocks = m_frameArrayLength / m_blockLength + 1;
	m_frameArrayLength = numBlocks * m_blockLength;
	
	// compensate for number of channels
	m_blockLength *= m_channels;
	m_frameArrayLength *= m_channels;
	m_intervalLength *= m_channels;

	m_pFrameArray = new uint16_t [m_frameArrayLength];
	
	for (size_t i = 0 ; i < m_frameArrayLength ; i++)
		m_pFrameArray[i] = 0;
	
	m_currentPos = 0;
	m_nextPos = m_intervalLength;
	m_distTime = m_interval;

	SDL_PauseAudio(0);
	m_init = true;

	return true;
}

bool MIPSDLAudioOutput::close()
{
	if (!m_init)
	{
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}

	SDL_PauseAudio(1);
	SDL_CloseAudio();

	delete [] m_pFrameArray;

	m_init = false;
	
	return true;
}

bool MIPSDLAudioOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
		pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 ))
	{
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (!m_init)
	{
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}
	
	MIPAudioMessage *audioMessage = (MIPAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}
	
	size_t num = audioMessage->getNumberOfFrames()*m_channels;
	size_t offset = 0;

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
		{
			uint16_t *pDest = m_pFrameArray + m_nextPos;
			const uint16_t *pSrc = frames + offset;
			
			memcpy(pDest, pSrc, amount*sizeof(uint16_t));
		}
		
		if (amount != num)
		{
			m_nextPos = 0;
//			std::cerr << "Cycling next pos" << std::endl;
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


bool MIPSDLAudioOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPSDLAUDIOOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return false;
}

void MIPSDLAudioOutput::StaticCallback(void *udata, uint8_t *stream, int len)
{
	MIPSDLAudioOutput *pInst = (MIPSDLAudioOutput *)udata;
	return pInst->Callback(stream, len);
}

void MIPSDLAudioOutput::Callback(uint8_t *stream, int len)
{
	if (len != m_blockLength*2) // 16 bit samples
		return;
	
	memcpy(stream, m_pFrameArray + m_currentPos, len);
	//SDL_MixAudio(stream, (uint8_t *)(m_pFrameArray + m_currentPos), len, SDL_MIX_MAXVOLUME);
	
	m_frameMutex.Lock();
	m_currentPos += m_blockLength;
	m_distTime -= m_blockTime;
	
	if (m_currentPos + m_blockLength > m_frameArrayLength)
	{
		m_currentPos = 0;
//		std::cerr << "Cycling" << std::endl;
	}
		
	if (m_nextPos < m_currentPos + m_blockLength && m_nextPos >= m_currentPos)
	{
		m_nextPos = (m_currentPos + m_intervalLength) % m_frameArrayLength;
		m_distTime = m_interval;
//		std::cerr << "Pushing next position" << std::endl;
	}
		
	if (m_distTime > MIPTime(0.200))
	{
		m_nextPos = (m_currentPos + m_intervalLength) % m_frameArrayLength;
		m_distTime = m_interval;
//		std::cerr << "Adjusting to runaway input (" << m_distTime.getString() << ")" << std::endl;
	}
	m_frameMutex.Unlock();		
}

#endif // MIPCONFIG_SUPPORT_SDLAUDIO

