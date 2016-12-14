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
#include "mipsamplingrateconverter.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#define MIPSAMPLINGRATECONVERTER_ERRSTR_NOTINIT				"Not initialized"
#define MIPSAMPLINGRATECONVERTER_ERRSTR_BADMESSAGETYPE			"Only floating point raw audio messages are allowed"
#define MIPSAMPLINGRATECONVERTER_ERRSTR_CANTHANDLECHANNELS		"Can't handle channel conversion"

MIPSamplingRateConverter::MIPSamplingRateConverter() : MIPComponent("MIPSamplingRateConverter")
{
	m_init = false;
}

MIPSamplingRateConverter::~MIPSamplingRateConverter()
{
	cleanUp();
}

bool MIPSamplingRateConverter::init(int outRate, int outChannels)
{
	if (m_init)
		cleanUp();

	m_outRate = outRate;
	m_outChannels = outChannels;

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
	std::list<MIPRawFloatAudioMessage *>::iterator it;

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
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT)) 
	{
		setErrorString(MIPSAMPLINGRATECONVERTER_ERRSTR_BADMESSAGETYPE);
		return false;
	}

	MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;
	
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
	
	const float *oldFrames = pAudioMsg->getFrames();
	float *newFrames = new float [numNewSamples];

	int outSampPos, i;
	float inFramePos, inFrameStep;

	inFrameStep = ((float)(numInFrames))/((float)numNewFrames);
	
	for (i = 0, outSampPos = 0, inFramePos = 0 ; i < numNewFrames ; i++, outSampPos += m_outChannels, inFramePos += inFrameStep)
	{
		int inFrameOffset = (int)inFramePos;
		float inFrameFrac = inFramePos-(float)inFrameOffset;
			
		if (m_outChannels == 1)
		{
			float val = 0;
			
			for (int j = 0 ; j < numInChannels ; j++)
			{
				if (inFrameOffset < numInFrames-1)
				{
					val += (1.0f - inFrameFrac) * oldFrames[inFrameOffset * numInChannels + j] +
					      inFrameFrac * oldFrames[(inFrameOffset + 1) * numInChannels + j];
				}
				else
					val += oldFrames[(numInFrames - 1) * numInChannels + j];
			}
			val /= (float)numInChannels;
			newFrames[outSampPos] = val;
		}
		else if (numInChannels == 1)
		{
			float val = 0;

			if (inFrameOffset < numInFrames-1)
			{
				val = (1.0f - inFrameFrac) * oldFrames[inFrameOffset] +
				      inFrameFrac * oldFrames[inFrameOffset + 1];
			}
			else
				val = oldFrames[numInFrames - 1];
				
			for (int j = 0 ; j < m_outChannels ; j++)
				newFrames[outSampPos + j] = val;
		}
		else // input channels == output channels
		{
			for (int j = 0 ; j < numInChannels ; j++)
			{
				float val = 0;

				if (inFrameOffset < numInFrames-1)
				{
					val = (1.0f - inFrameFrac) * oldFrames[inFrameOffset * numInChannels + j] +
					      inFrameFrac * oldFrames[(inFrameOffset + 1) * numInChannels + j];
				}
				else
					val = oldFrames[(numInFrames - 1) * numInChannels + j];
				
				newFrames[outSampPos+j] = val;
			}
		}
	}

	MIPRawFloatAudioMessage *pNewMsg;
	
	pNewMsg = new MIPRawFloatAudioMessage(m_outRate, m_outChannels, numNewFrames, newFrames, true);
	pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time info and source ID
	
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

