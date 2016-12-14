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
#include "mipaudiosplitter.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#define MIPAUDIOSPLITTER_ERRSTR_NOTINIT							"Not initialized"
#define MIPAUDIOSPLITTER_ERRSTR_BADMESSAGE						"Bad message"

MIPAudioSplitter::MIPAudioSplitter() : MIPComponent("MIPAudioSplitter")
{
	m_init = false;
}

MIPAudioSplitter::~MIPAudioSplitter()
{
	cleanUp();
}

bool MIPAudioSplitter::init(MIPTime interval)
{
	if (m_init)
		cleanUp();
	m_interval = interval;
	m_msgIt = m_messages.begin();
	m_prevIteration = -1;
	m_init = true;
	return true;
}

void MIPAudioSplitter::cleanUp()
{
	if (!m_init)
		return;
	clearMessages();
	m_init = false;
}

void MIPAudioSplitter::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

bool MIPAudioSplitter::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSPLITTER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (pMsg->getMessageType() != MIPMESSAGE_TYPE_AUDIO_RAW)
	{
		setErrorString(MIPAUDIOSPLITTER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPAudioMessage *pRawAudioMsg = (MIPAudioMessage *)pMsg;
	
	int sampRate = pRawAudioMsg->getSamplingRate();
	int numChannels = pRawAudioMsg->getNumberOfChannels();
	int numFrames = pRawAudioMsg->getNumberOfFrames();
	int numFramesLeft = numFrames;
	int intervalFrames = (int)((m_interval.getValue()*(real_t)sampRate)+0.5);
	MIPTime t = pRawAudioMsg->getTime();
	uint64_t sourceID = pRawAudioMsg->getSourceID();
	
	uint32_t subtype = pMsg->getMessageSubtype();
	if (subtype == MIPRAWAUDIOMESSAGE_TYPE_FLOAT)
	{
		MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;
		const float *pInputFrames = pAudioMsg->getFrames();
		while (numFramesLeft > 0)
		{
			int num = intervalFrames;
			if (num > numFramesLeft)
				num = numFramesLeft;
			int numSamples = num * numChannels;
			
			float *pOutputFrames = new float [numSamples];

			memcpy(pOutputFrames, pInputFrames, numSamples*sizeof(float));

			MIPRawFloatAudioMessage *pNewMsg = new MIPRawFloatAudioMessage(sampRate, numChannels, num, pOutputFrames, true);
			pNewMsg->setTime(t);
			pNewMsg->setSourceID(sourceID);
			m_messages.push_back(pNewMsg);

			numFramesLeft -= num;
			pInputFrames += numSamples;
			t += m_interval;
		}
		m_msgIt = m_messages.begin();
	}
	else if (subtype == MIPRAWAUDIOMESSAGE_TYPE_U8)
	{
		MIPRawU8AudioMessage *pAudioMsg = (MIPRawU8AudioMessage *)pMsg;
		const uint8_t *pInputFrames = pAudioMsg->getFrames();
		while (numFramesLeft > 0)
		{
			int num = intervalFrames;
			if (num > numFramesLeft)
				num = numFramesLeft;
			int numSamples = num * numChannels;
			
			uint8_t *pOutputFrames = new uint8_t [numSamples];

			memcpy(pOutputFrames, pInputFrames, numSamples*sizeof(uint8_t));

			MIPRawU8AudioMessage *pNewMsg = new MIPRawU8AudioMessage(sampRate, numChannels, num, pOutputFrames, true);
			pNewMsg->setTime(t);
			pNewMsg->setSourceID(sourceID);
			m_messages.push_back(pNewMsg);

			numFramesLeft -= num;
			pInputFrames += numSamples;
			t += m_interval;
		}
		m_msgIt = m_messages.begin();
	}
	else if (subtype == MIPRAWAUDIOMESSAGE_TYPE_S16LE || subtype == MIPRAWAUDIOMESSAGE_TYPE_U16LE ||
		 subtype == MIPRAWAUDIOMESSAGE_TYPE_S16BE || subtype == MIPRAWAUDIOMESSAGE_TYPE_U16BE ||
		 subtype == MIPRAWAUDIOMESSAGE_TYPE_S16 || subtype == MIPRAWAUDIOMESSAGE_TYPE_U16 )
	{
		MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
		const uint16_t *pInputFrames = pAudioMsg->getFrames();
		MIPRaw16bitAudioMessage::SampleEncoding sampEnc = pAudioMsg->getSampleEncoding();
		bool isSigned = pAudioMsg->isSigned();
		
		while (numFramesLeft > 0)
		{
			int num = intervalFrames;
			if (num > numFramesLeft)
				num = numFramesLeft;
			int numSamples = num * numChannels;
			
			uint16_t *pOutputFrames = new uint16_t [numSamples];

			memcpy(pOutputFrames, pInputFrames, numSamples*sizeof(uint16_t));

			MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(sampRate, numChannels, num, isSigned, sampEnc, pOutputFrames, true);
			pNewMsg->setTime(t);
			pNewMsg->setSourceID(sourceID);
			m_messages.push_back(pNewMsg);

			numFramesLeft -= num;
			pInputFrames += numSamples;
			t += m_interval;
		}
		m_msgIt = m_messages.begin();
	}
	else
	{
		setErrorString(MIPAUDIOSPLITTER_ERRSTR_BADMESSAGE);
		return false;
	}

	return true;
}

bool MIPAudioSplitter::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSPLITTER_ERRSTR_NOTINIT);
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

