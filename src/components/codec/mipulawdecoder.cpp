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
#include "mipulawdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#define MIPULAWDECODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPULAWDECODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPULAWDECODER_ERRSTR_BADMESSAGE			"Only u-law encoded audio messages are accepted"

#define MIPULAWDECODER_BIAS					132

MIPULawDecoder::MIPULawDecoder() : MIPComponent("MIPULawEncoder")
{
	m_init = false;
}

MIPULawDecoder::~MIPULawDecoder()
{
	destroy();
}

bool MIPULawDecoder::init()
{
	if (m_init)
	{
		setErrorString(MIPULAWDECODER_ERRSTR_ALREADYINIT);
		return false;
	}
	
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_init = true;
	
	return true;
}

bool MIPULawDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPULAWDECODER_ERRSTR_NOTINIT);
		return false;
	}
	
	clearMessages();
	m_init = false;

	return true;
}

bool MIPULawDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPULAWDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_ULAW) ) 
	{
		setErrorString(MIPULAWDECODER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPEncodedAudioMessage *pAudioMsg = (MIPEncodedAudioMessage *)pMsg;
	int numFrames = pAudioMsg->getNumberOfFrames();
	int numChannels = pAudioMsg->getNumberOfChannels();
	int numBytes =(int)pAudioMsg->getDataLength();
	int sampRate = pAudioMsg->getSamplingRate();

	if (numBytes != numFrames*numChannels)
	{
		// something's wrong, ignore it
		return true;
	}
	
	int16_t *pSamples = (int16_t *)new uint16_t[numBytes];
	const uint8_t *pBuffer = pAudioMsg->getData();

	for (int i = 0 ; i < numBytes ; i++)
	{
		int val = (int)(~pBuffer[i]);
		int mant,sign,expon;
		
		sign = val&(1<<7);
		mant = val&15;
		expon = (val>>4)&7;

		val = (1<<(7+expon));
		val |= (mant<<(3+expon));
		val -= MIPULAWDECODER_BIAS;
		if (val < 0)
			val = 0;
		if (sign != 0)
			val = -val;
		if (val > 32767)
			val = 32767;
		else if (val < -32768)
			val = -32768;
		
		pSamples[i] = (int16_t)val;
	}
	
	MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(sampRate, numChannels, numFrames, true, MIPRaw16bitAudioMessage::Native, (uint16_t *)pSamples, true);
	pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
	m_messages.push_back(pNewMsg);
		
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPULawDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPULAWDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (m_msgIt == m_messages.end())
	{
		*pMsg = 0;
		m_msgIt = m_messages.begin();
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	
	return true;
}

void MIPULawDecoder::clearMessages()
{
	std::list<MIPRaw16bitAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

