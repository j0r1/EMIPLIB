/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_AUDIOTRACK

#include "mipaudiotrackoutput.h"
#include "miprawaudiomessage.h"
#include <media/AudioTrack.h>
#include <iostream>

#include "mipdebug.h"

#define MIPAUDIOTRACKOUTPUT_ERRSTR_CLIENTALREADYOPEN		"Already opened a client"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_CANTOPENCLIENT		"Error opening client"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"Pull not implemented"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_CANTREGISTERCHANNEL	"Unable to register a new port"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_ARRAYTIMETOOSMALL		"Array time too small"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_CLIENTNOTOPEN		"Client is not running"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_BADMESSAGE			"Not a floating point raw audio message"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_NOTSTEREO			"Not stereo audio"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_CANTGETPHYSICALPORTS		"Unable to obtain physical ports"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_NOTENOUGHPORTS		"Less than two ports were found, not enough for stereo"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_CANTCONNECTPORTS		"Unable to connect ports"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_CANTACTIVATECLIENT		"Can't activate client"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_INVALIDCHANNELS		"Only 1 or 2 channels are supported"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_CANTGETCHANNELS		"Unable to obtain the requested number of channels"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_INCOMPATIBLECHANNELS		"Received message contains the wrong number of channels"
#define MIPAUDIOTRACKOUTPUT_ERRSTR_INTERVALTOOSMALL		"Specified interval is too small, should be larger than "

MIPAudioTrackOutput::MIPAudioTrackOutput() : MIPComponent("MIPAudioTrackOutput"), m_delay(0), m_distTime(0), m_blockTime(0), m_interval(0)
{
	int status;
	
	m_pTrack = 0;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize AudioTrack callback mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPAudioTrackOutput::~MIPAudioTrackOutput()
{
	close();
}

bool MIPAudioTrackOutput::open(int sampRate, int channels, MIPTime interval, MIPTime arrayTime)
{
	if (m_pTrack != 0)
	{
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_CLIENTALREADYOPEN);
		return false;
	}
	
	if (arrayTime.getValue() < 2.0) // Should be at least two since one second will be filled in 
	{                               // in the close function
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_ARRAYTIMETOOSMALL);
		return false;
	}

	if (!(channels == 1 || channels == 2))
	{
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_INVALIDCHANNELS);
		return false;
	}
	
	m_pTrack = new android::AudioTrack();
	if (m_pTrack->set(android::AudioSystem::DEFAULT, sampRate, android::AudioSystem::PCM_16_BIT,
	                  channels, 0, 0, StaticAudioTrackCallback, this, 0) != 0)
	{
		delete m_pTrack;
		m_pTrack = 0;
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_CANTOPENCLIENT);
		return false;
	}
	
	// TODO: check sampling rate somehow?

	if (channels != m_pTrack->channelCount())
	{
		delete m_pTrack;
		m_pTrack = 0;
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_CANTGETCHANNELS);
		return false;
	}

	m_sampRate = sampRate;
	m_channels = channels;

//	m_blockFrames = m_pTrack->frameCount() / 2; // TODO: better way to obtain notificationFrames?
	m_blockFrames = m_pTrack->frameCount();

	size_t frames = (size_t)(interval.getValue()*(real_t)m_sampRate+0.5);

	if (frames < m_blockFrames)
	{
		delete m_pTrack;
		m_pTrack = 0;

		char str[256];
		real_t minInterval = ((real_t)m_blockFrames)/((real_t)m_sampRate);

		MIP_SNPRINTF(str, 255, "%f", (double)minInterval);

		setErrorString(std::string(MIPAUDIOTRACKOUTPUT_ERRSTR_INTERVALTOOSMALL) + std::string(str));
		return false;
	}

	m_blockLength = m_blockFrames * m_channels;
	m_blockTime = MIPTime((real_t)m_blockFrames/(real_t)m_sampRate);
	m_frameArrayLength = ((size_t)((arrayTime.getValue() * ((real_t)m_sampRate)) + 0.5));
	m_intervalLength = ((((size_t)((interval.getValue()*(real_t)m_sampRate)+0.5))/m_blockLength)+1)*m_blockLength;
	m_interval = MIPTime((real_t)m_intervalLength/(real_t)m_sampRate);

	if (m_intervalLength > m_frameArrayLength/4)
	{
		delete m_pTrack;
		m_pTrack = 0;
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_ARRAYTIMETOOSMALL);
		return false;
	}

	// make sure a fixed number of blocks fit in the frame array
	size_t numBlocks = m_frameArrayLength / m_blockLength + 1;
	m_frameArrayLength = numBlocks * m_blockLength * m_channels;
	
	m_pFrameArray = new uint16_t[m_frameArrayLength];
	
	for (size_t i = 0 ; i < m_frameArrayLength ; i++)
		m_pFrameArray[i] = 0;
	
	m_currentPos = 0;
	m_nextPos = m_intervalLength;
	m_distTime = m_interval;

	m_pTrack->start();

	return true;
}

bool MIPAudioTrackOutput::close()
{
	if (m_pTrack == 0)
	{
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_CLIENTNOTOPEN);
		return false;
	}

	delete m_pTrack;
	m_pTrack = 0;

	delete [] m_pFrameArray;

	return true;
}

bool MIPAudioTrackOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
		pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16))
	{
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pTrack == 0)
	{
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_CLIENTNOTOPEN);
		return false;
	}
	
	MIPAudioMessage *audioMessage = (MIPAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}
	
	size_t num = audioMessage->getNumberOfFrames() * m_channels;
	size_t offset = 0;

	MIPRaw16bitAudioMessage *audioMessage16bit = (MIPRaw16bitAudioMessage *)pMsg;
	const uint16_t *frames = audioMessage16bit->getFrames();

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
			size_t i;
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

bool MIPAudioTrackOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPAUDIOTRACKOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return false;
}

void MIPAudioTrackOutput::StaticAudioTrackCallback(int event, void *pUser, void *pInfo)
{
	if (event == android::AudioTrack::EVENT_MORE_DATA)
	{
		MIPAudioTrackOutput *pInst = (MIPAudioTrackOutput *)pUser;
		pInst->AudioTrackCallback(pInfo);
	}
}

void MIPAudioTrackOutput::AudioTrackCallback(void *pInfo)
{
	android::AudioTrack::Buffer *pBuffer = (android::AudioTrack::Buffer *)pInfo;

	size_t blockLength = pBuffer->frameCount * m_channels;
	MIPTime blockTime(((real_t)pBuffer->frameCount)/((real_t)m_sampRate));
	
	memcpy(pBuffer->i16, m_pFrameArray + m_currentPos, blockLength*sizeof(uint16_t));
	
	m_frameMutex.Lock();
	m_currentPos += blockLength;
	m_distTime -= blockTime;
	
	if (m_currentPos + blockLength >= m_frameArrayLength)
	{
		m_currentPos = 0;
//		std::cerr << "Cycling" << std::endl;
	}
		
	if (m_nextPos < m_currentPos + blockLength && m_nextPos >= m_currentPos)
	{
		m_nextPos = (m_currentPos + m_intervalLength) % m_frameArrayLength;
		m_distTime = m_interval;
//		std::cerr << "Pushing next position" << std::endl;
	}
		
	if (m_distTime > MIPTime(0.800)) // TODO: make this configurable?
	{
		m_nextPos = (m_currentPos + m_intervalLength) % m_frameArrayLength;
		m_distTime = m_interval;
//		std::cerr << "Adjusting to runaway input (" << m_distTime.getString() << ")" << std::endl;
	}
	m_frameMutex.Unlock();		
}

#endif // MIPCONFIG_SUPPORT_AUDIOTRACK

