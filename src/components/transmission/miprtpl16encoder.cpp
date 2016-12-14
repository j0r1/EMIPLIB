/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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
#include "miprtpl16encoder.h"
#include "miprtpmessage.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"

#define MIPRTPL16ENCODER_ERRSTR_BADMESSAGE			"Can't understand message"
#define MIPRTPL16ENCODER_ERRSTR_NOTINIT				"RTP U-law encoder not initialized"
#define MIPRTPL16ENCODER_ERRSTR_BADSAMPLINGRATE			"Only a sampling rate of 44100Hz is allowed"
#define MIPRTPL16ENCODER_ERRSTR_BADCHANNELS			"Message with a wrong number of channels was received"

MIPRTPL16Encoder::MIPRTPL16Encoder() : MIPRTPEncoder("MIPRTPL16Encoder")
{
	m_init = false;
}

MIPRTPL16Encoder::~MIPRTPL16Encoder()
{
	cleanUp();
}

bool MIPRTPL16Encoder::init(bool stereo)
{
	if (m_init)
		cleanUp();

	if (stereo)
		m_channels = 2;
	else
		m_channels = 1;

	m_prevIteration = -1;
	m_init = true;
	return true;
}

bool MIPRTPL16Encoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPL16ENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16BE))
	{
		setErrorString(MIPRTPL16ENCODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	MIPRaw16bitAudioMessage *pRawMsg = (MIPRaw16bitAudioMessage *)pMsg;
	int sampRate = pRawMsg->getSamplingRate();
	int channels = pRawMsg->getNumberOfChannels();
	
	if (sampRate != 44100)
	{
		setErrorString(MIPRTPL16ENCODER_ERRSTR_BADSAMPLINGRATE);
		return false;
	}
	if (channels != m_channels)
	{
		setErrorString(MIPRTPL16ENCODER_ERRSTR_BADCHANNELS);
		return false;
	}
	
	const uint16_t *pFrames = pRawMsg->getFrames();
	bool marker = false;
	size_t length = pRawMsg->getNumberOfFrames() * m_channels * sizeof(uint16_t);
	uint8_t *pPayload = new uint8_t [length];

	memcpy(pPayload,pFrames,length);

	MIPRTPSendMessage *pNewMsg;

	pNewMsg = new MIPRTPSendMessage(pPayload,length,getPayloadType(),marker,pRawMsg->getNumberOfFrames());
	pNewMsg->setSamplingInstant(pRawMsg->getTime());
	
	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
		
	return true;
}

bool MIPRTPL16Encoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPL16ENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (m_msgIt == m_messages.end())
	{
		m_msgIt = m_messages.begin();
		*pMsg = 0;
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	return true;
}

void MIPRTPL16Encoder::cleanUp()
{
	clearMessages();
	m_init = false;
}

void MIPRTPL16Encoder::clearMessages()
{
	std::list<MIPRTPSendMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

