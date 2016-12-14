/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Hasselt University - Expertise Centre for
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
#include "mipsamplingrateconverter.h"
#include "miprawaudiomessage.h"
#include "mipresample.h"

#include "mipdebug.h"

#define MIPSAMPLINGRATECONVERTER_ERRSTR_NOTINIT				"Not initialized"
#define MIPSAMPLINGRATECONVERTER_ERRSTR_BADMESSAGETYPE			"Only floating point raw audio messages are allowed"
#define MIPSAMPLINGRATECONVERTER_ERRSTR_CANTHANDLECHANNELS		"Can't handle channel conversion"
#define MIPSAMPLINGRATECONVERTER_ERRSTR_CANTRESAMPLE			"Unable to resample the data"

MIPSamplingRateConverter::MIPSamplingRateConverter() : MIPComponent("MIPSamplingRateConverter")
{
	m_init = false;
}

MIPSamplingRateConverter::~MIPSamplingRateConverter()
{
	cleanUp();
}

bool MIPSamplingRateConverter::init(int outRate, int outChannels, bool floatSamples)
{
	if (m_init)
		cleanUp();

	m_outRate = outRate;
	m_outChannels = outChannels;
	m_floatSamples = floatSamples;

	m_prevIteration = -1;
	m_init = true;
	
	return true;
}

void MIPSamplingRateConverter::cleanUp()
{
	if (!m_init)
		return;

	clearMessages();
	m_init = false;
}

void MIPSamplingRateConverter::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

bool MIPSamplingRateConverter::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSAMPLINGRATECONVERTER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
		( (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT && m_floatSamples) ||
		  (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 && !m_floatSamples) ) ) ) 
	{
		setErrorString(MIPSAMPLINGRATECONVERTER_ERRSTR_BADMESSAGETYPE);
		return false;
	}

	MIPAudioMessage *pAudioMsg = (MIPAudioMessage *)pMsg;
	
	if (pAudioMsg->getNumberOfChannels() != 1 && m_outChannels != pAudioMsg->getNumberOfChannels() && m_outChannels != 1)
	{
		setErrorString(MIPSAMPLINGRATECONVERTER_ERRSTR_CANTHANDLECHANNELS);
		return false;
	}
	
	int numInChannels = pAudioMsg->getNumberOfChannels();
	int numInFrames = pAudioMsg->getNumberOfFrames();
	real_t frameTime = (((real_t)numInFrames)/((real_t)pAudioMsg->getSamplingRate()));
	int numNewFrames = (int)((frameTime * ((real_t)m_outRate))+0.5);
	int numNewSamples = numNewFrames * m_outChannels;
	MIPAudioMessage *pNewMsg = 0;
	
	if (m_floatSamples)
	{
		MIPRawFloatAudioMessage *pFloatAudioMsg = (MIPRawFloatAudioMessage *)pMsg;
		const float *oldFrames = pFloatAudioMsg->getFrames();
		float *newFrames = new float [numNewSamples];
	
		if (!MIPResample<float,float>(oldFrames, numInFrames, numInChannels, newFrames, numNewFrames, m_outChannels))
		{
			setErrorString(MIPSAMPLINGRATECONVERTER_ERRSTR_CANTRESAMPLE);
			return false;
		}
		
		pNewMsg = new MIPRawFloatAudioMessage(m_outRate, m_outChannels, numNewFrames, newFrames, true);
		pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time info and source ID
	}
	else // 16 bit signed
	{
		MIPRaw16bitAudioMessage *pIntAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
		const uint16_t *oldFrames = pIntAudioMsg->getFrames();
		uint16_t *newFrames = new uint16_t [numNewSamples];
	
		if (!MIPResample<int16_t,int32_t>((const int16_t *)oldFrames, numInFrames, numInChannels, (int16_t *)newFrames, numNewFrames, m_outChannels))
		{
			setErrorString(MIPSAMPLINGRATECONVERTER_ERRSTR_CANTRESAMPLE);
			return false;
		}
		
		pNewMsg = new MIPRaw16bitAudioMessage(m_outRate, m_outChannels, numNewFrames, true, MIPRaw16bitAudioMessage::Native, newFrames, true);
		pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time info and source ID
	}
	
	if (m_prevIteration != iteration)
	{
		clearMessages();
		m_prevIteration = iteration;
	}
	
	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPSamplingRateConverter::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSAMPLINGRATECONVERTER_ERRSTR_NOTINIT);
		return false;
	}

	if (m_prevIteration != iteration)
	{
		clearMessages();
		m_prevIteration = iteration;
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

