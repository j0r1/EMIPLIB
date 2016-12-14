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
#include "mipalawdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#define MIPALAWDECODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPALAWDECODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPALAWDECODER_ERRSTR_BADMESSAGE			"Only a-law encoded audio messages are accepted"

MIPALawDecoder::MIPALawDecoder() : MIPComponent("MIPALawEncoder")
{
	m_init = false;
}

MIPALawDecoder::~MIPALawDecoder()
{
	destroy();
}

bool MIPALawDecoder::init()
{
	if (m_init)
	{
		setErrorString(MIPALAWDECODER_ERRSTR_ALREADYINIT);
		return false;
	}
	
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_init = true;
	
	return true;
}

bool MIPALawDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPALAWDECODER_ERRSTR_NOTINIT);
		return false;
	}
	
	clearMessages();
	m_init = false;

	return true;
}

bool MIPALawDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPALAWDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_ALAW) ) 
	{
		setErrorString(MIPALAWDECODER_ERRSTR_BADMESSAGE);
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
		int samp, mant, seg, sign;
		int pcm = pBuffer[i];

		pcm ^= 0x55;
		mant = pcm & 0xF;
		seg = (pcm >> 4) & 0x7;
        		
		if(seg > 0)
			mant |= 0x10;

		sign = !(pcm >> 7);
		
		samp = ((mant << 1) + 1);

		if(seg > 1)
			samp <<= seg - 1;

		samp = sign ? -samp : samp;

		if (samp < -32767)
			samp = -32767;
		else if (samp > 32767)
			samp = 32767;

		pSamples[i] = (int16_t)samp;
	}
	
	MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(sampRate, numChannels, numFrames, true, MIPRaw16bitAudioMessage::Native, (uint16_t *)pSamples, true);
	pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
	m_messages.push_back(pNewMsg);
		
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPALawDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPALAWDECODER_ERRSTR_NOTINIT);
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

void MIPALawDecoder::clearMessages()
{
	std::list<MIPRaw16bitAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

