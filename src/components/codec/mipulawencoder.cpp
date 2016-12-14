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
#include "mipulawencoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#define MIPULAWENCODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPULAWENCODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPULAWENCODER_ERRSTR_BADMESSAGE			"Only signed 16 bit native endian raw audio messages are accepted"

const uint8_t MIPULawEncoder::expEnc[256] = 
{
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

#define MIPULAWENCODER_CLIP						32635
#define MIPULAWENCODER_BIAS						132


MIPULawEncoder::MIPULawEncoder() : MIPComponent("MIPULawEncoder")
{
	m_init = false;
}

MIPULawEncoder::~MIPULawEncoder()
{
	destroy();
}

bool MIPULawEncoder::init()
{
	if (m_init)
	{
		setErrorString(MIPULAWENCODER_ERRSTR_ALREADYINIT);
		return false;
	}
	
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_init = true;
	
	return true;
}

bool MIPULawEncoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPULAWENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	clearMessages();
	m_init = false;

	return true;
}

bool MIPULawEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPULAWENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16) ) 
	{
		setErrorString(MIPULAWENCODER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
	int numFrames = pAudioMsg->getNumberOfFrames();
	int numChannels = pAudioMsg->getNumberOfChannels();
	int numBytes = numFrames*numChannels;
	int sampRate = pAudioMsg->getSamplingRate();
	const int16_t *pSamples = (const int16_t *)pAudioMsg->getFrames();
	uint8_t *pBuffer = new uint8_t[numBytes];

	for (int i = 0 ; i < numBytes ; i++)
	{
		int val = pSamples[i];
		int mant,signval,mantpos,expon;
		
		if (val < 0)
		{
			signval = (1<<7);
			if (val < -MIPULAWENCODER_CLIP)
				val = MIPULAWENCODER_CLIP;
			else
				val = -val;
		}
		else
		{
			signval = 0;
			if (val > MIPULAWENCODER_CLIP)
				val = MIPULAWENCODER_CLIP;
		}

		val += MIPULAWENCODER_BIAS;
		expon = expEnc[val>>7];
		mantpos = (int)expon + 7 - 4;
		mant = ((val >> mantpos) & 15);
		pBuffer[i] = ~((uint8_t)(signval|(expon<<4)|mant));
	}
	
	MIPEncodedAudioMessage *pNewMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_ULAW, sampRate, numChannels, numFrames, pBuffer, numBytes, true);
	pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
	m_messages.push_back(pNewMsg);
		
	m_msgIt = m_messages.begin();

	return true;
}

bool MIPULawEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPULAWENCODER_ERRSTR_NOTINIT);
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

void MIPULawEncoder::clearMessages()
{
	std::list<MIPEncodedAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

