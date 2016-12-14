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
#include "mipaudiomixer.h"
#include "miprawaudiomessage.h"
#include "mipfeedback.h"

#include "mipdebug.h"

#define MIPAUDIOMIXER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPAUDIOMIXER_ERRSTR_NOTINIT				"Not initialized"
#define MIPAUDIOMIXER_ERRSTR_BADMESSAGE				"Bad message"
#define MIPAUDIOMIXER_ERRSTR_INCOMPATIBLESAMPRATE		"Sampling rate differs from value set during initialization"
#define MIPAUDIOMIXER_ERRSTR_INCOMPATIBLECHANNELS		"Number of channels differs from value set during initialization"
#define MIPAUDIOMIXER_ERRSTR_CANTSETPLAYBACKTIME		"Can't store playback stream time in feedback message"
#define MIPAUDIOMIXER_ERRSTR_DELAYTOOLARGE			"The specified extra delay is too large"
#define MIPAUDIOMIXER_ERRSTR_TIMINGINFOISNOTUSED		"Timing information is not being used, so the extra delay will not have any effect"
#define MIPAUDIOMIXER_ERRSTR_NEGATIVEDELAY			"Only positive delays are allowed"

MIPAudioMixer::MIPAudioMixer() : MIPComponent("MIPAudioMixer"), m_blockTime(0), m_playTime(0)
{
	m_init = false;
}

MIPAudioMixer::~MIPAudioMixer()
{
	destroy();
}

bool MIPAudioMixer::init(int sampRate, int channels, MIPTime blockTime, bool useTimeInfo, bool floatSamples)
{
	if (m_init)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_sampRate = sampRate;
	m_channels = channels;
	m_blockTime = blockTime;
	m_blockFrames = (size_t)(blockTime.getValue()*((real_t)sampRate)+0.5);
	m_blockSize = m_blockFrames*(size_t)channels;
	m_playTime = MIPTime(0);
	m_curInterval = 0;
	m_useTimeInfo = useTimeInfo;
	m_floatSamples = floatSamples;

	if (floatSamples)
	{
		m_pSilenceFramesFloat = new float [m_blockSize];
		for (size_t i = 0 ; i < m_blockSize ; i++)
			m_pSilenceFramesFloat[i] = 0;
		
		m_pMsgFloat = new MIPRawFloatAudioMessage(sampRate, channels, (int)m_blockFrames, m_pSilenceFramesFloat, false);
		m_pMsgInt = 0;
		m_pSilenceFramesInt = 0;
	}
	else
	{
		m_pSilenceFramesInt = new uint16_t [m_blockSize];
		for (size_t i = 0 ; i < m_blockSize ; i++)
			m_pSilenceFramesInt[i] = 0;
		
		m_pMsgInt = new MIPRaw16bitAudioMessage(sampRate, channels, (int)m_blockFrames, true, MIPRaw16bitAudioMessage::Native, m_pSilenceFramesInt, false);
		m_pMsgFloat = 0;
		m_pSilenceFramesFloat = 0;
	}
	
	m_pBlockFramesFloat = 0;
	m_pBlockFramesInt = 0;
	
	m_extraDelay = MIPTime(0);	
	
	m_prevIteration = -1;
	m_init = true;
	
	return true;
}

bool MIPAudioMixer::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_NOTINIT);
		return false;
	}

	clearAudioBlocks();
	if (m_pMsgFloat)
		delete m_pMsgFloat;
	if (m_pBlockFramesFloat)
		delete [] m_pBlockFramesFloat;
	if (m_pSilenceFramesFloat)
		delete [] m_pSilenceFramesFloat;
	if (m_pMsgInt)
		delete m_pMsgInt;
	if (m_pBlockFramesInt)
		delete [] m_pBlockFramesInt;
	if (m_pSilenceFramesInt)
		delete [] m_pSilenceFramesInt;

	m_init = false;
	
	return true;
}

bool MIPAudioMixer::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_NOTINIT);
		return false;
	}
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && ((pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT && m_floatSamples) || (!m_floatSamples && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 ) )))
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPAudioMessage *pAudioMsg = (MIPAudioMessage *)pMsg;

	if (pAudioMsg->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_INCOMPATIBLESAMPRATE);
		return false;
	}
	if (pAudioMsg->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}

	real_t block = 0;

	if (m_useTimeInfo)
	{
		MIPTime msgTime = pAudioMsg->getTime();
		if (msgTime < m_playTime) // frames are too old to be played. Ignore
			return true;
		
		MIPTime offsetTime = msgTime;
		offsetTime -= m_playTime;

		if (offsetTime.getValue() > 300.0) // if it's more than five minutes in the future, ignore it
			return true;

		offsetTime += m_extraDelay;
		
		block = offsetTime.getValue()/m_blockTime.getValue();
	}
	
	// Data is ok, process it

	int64_t blockOffset = (int64_t)block;
	int64_t intervalNumber = blockOffset + m_curInterval;
	real_t blockFraction = block - (real_t)blockOffset;
	size_t frameOffset = (size_t)((blockFraction * (real_t)m_blockFrames) + 0.5);
	size_t sampleOffset = frameOffset * m_channels;
	size_t numSamplesLeft = (size_t)(pAudioMsg->getNumberOfFrames())*m_channels;
	size_t samplePos = 0;

	if (m_floatSamples)
	{
		MIPRawFloatAudioMessage *pFloatAudioMsg = (MIPRawFloatAudioMessage *)pMsg;
		const float *pSamples = pFloatAudioMsg->getFrames();
	
		initBlockSearch();
		while (numSamplesLeft != 0)
		{
			MIPAudioMixerBlock block = searchBlock(intervalNumber);
			float *blockSamples = block.getFramesFloat();
			
			size_t num = (numSamplesLeft > (m_blockSize-sampleOffset))?(m_blockSize-sampleOffset):numSamplesLeft;
			size_t i,j,k;
			
			// add samples to the block
			for (i = 0, j = sampleOffset, k = samplePos ; i < num ; i++, j++, k++)
				blockSamples[j] += pSamples[k];
			
			sampleOffset = 0;
			samplePos += num;
			numSamplesLeft -= num;
			intervalNumber++;
		}
	}
	else // signed 16 bit
	{
		MIPRaw16bitAudioMessage *pIntAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
		const int16_t *pSamples = (const int16_t *)pIntAudioMsg->getFrames();
	
		initBlockSearch();
		while (numSamplesLeft != 0)
		{
			MIPAudioMixerBlock block = searchBlock(intervalNumber);
			int16_t *blockSamples = (int16_t *)block.getFramesInt();
			
			size_t num = (numSamplesLeft > (m_blockSize-sampleOffset))?(m_blockSize-sampleOffset):numSamplesLeft;
			size_t i,j,k;
			
			// add samples to the block
			for (i = 0, j = sampleOffset, k = samplePos ; i < num ; i++, j++, k++)
				blockSamples[j] += pSamples[k];
			
			sampleOffset = 0;
			samplePos += num;
			numSamplesLeft -= num;
			intervalNumber++;
		}
	}
	
	return true;
}

bool MIPAudioMixer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_NOTINIT);
		return false;
	}

	if (m_floatSamples)
	{
		if (m_prevIteration != 	iteration)
		{
			m_prevIteration = iteration;
			
			// delete old frames
			if (m_pBlockFramesFloat)
				delete [] m_pBlockFramesFloat;
			m_pBlockFramesFloat = 0;
			
			if (m_audioBlocks.empty())
				m_pMsgFloat->setFrames(m_pSilenceFramesFloat,false);
			else
			{
				MIPAudioMixerBlock block = *(m_audioBlocks.begin());
	
				if (block.getInterval() == m_curInterval)
				{
					m_audioBlocks.pop_front();
					m_pBlockFramesFloat = block.getFramesFloat();
					m_pMsgFloat->setFrames(m_pBlockFramesFloat,false);
				}
				else
					m_pMsgFloat->setFrames(m_pSilenceFramesFloat,false);
			}
			
			m_gotMessage = false;
			m_curInterval++;
			m_playTime += m_blockTime;
		}
		
		if (m_gotMessage)
		{
			m_gotMessage = false;
			*pMsg = 0;
		}
		else
		{
			m_gotMessage = true;
			*pMsg = m_pMsgFloat;
		}
	}
	else // 16 bit signed
	{
		if (m_prevIteration != 	iteration)
		{
			m_prevIteration = iteration;
			
			// delete old frames
			if (m_pBlockFramesInt)
				delete [] m_pBlockFramesInt;
			m_pBlockFramesInt = 0;
			
			if (m_audioBlocks.empty())
				m_pMsgInt->setFrames(true, MIPRaw16bitAudioMessage::Native, m_pSilenceFramesInt,false);
			else
			{
				MIPAudioMixerBlock block = *(m_audioBlocks.begin());
	
				if (block.getInterval() == m_curInterval)
				{
					m_audioBlocks.pop_front();
					m_pBlockFramesInt = block.getFramesInt();
					m_pMsgInt->setFrames(true, MIPRaw16bitAudioMessage::Native, m_pBlockFramesInt,false);
				}
				else
					m_pMsgInt->setFrames(true, MIPRaw16bitAudioMessage::Native, m_pSilenceFramesInt,false);
			}
			
			m_gotMessage = false;
			m_curInterval++;
			m_playTime += m_blockTime;
		}
		
		if (m_gotMessage)
		{
			m_gotMessage = false;
			*pMsg = 0;
		}
		else
		{
			m_gotMessage = true;
			*pMsg = m_pMsgInt;
		}
	}
	return true;
}

void MIPAudioMixer::clearAudioBlocks()
{
	std::list<MIPAudioMixerBlock>::iterator it;

	for (it = m_audioBlocks.begin() ; it != m_audioBlocks.end() ; it++)
	{
		if ((*it).getFramesFloat())
			delete [] (*it).getFramesFloat();
		if ((*it).getFramesInt())
			delete [] (*it).getFramesInt();
	}
	m_audioBlocks.clear();
}

void MIPAudioMixer::initBlockSearch()
{
	m_it = m_audioBlocks.begin();
}

MIPAudioMixer::MIPAudioMixerBlock MIPAudioMixer::searchBlock(int64_t intervalNumber)
{
	if (m_it == m_audioBlocks.end())
	{
		if (m_floatSamples)
		{
			float *pNewFrames = new float [m_blockSize];
			
			memcpy(pNewFrames, m_pSilenceFramesFloat, sizeof(float)*m_blockSize);
			MIPAudioMixerBlock block(intervalNumber, pNewFrames);
			m_audioBlocks.push_back(block);
			m_it = m_audioBlocks.end();
			return block;
		}

		// 16 bit signed
		
		uint16_t *pNewFrames = new uint16_t [m_blockSize];
			
		memcpy(pNewFrames, m_pSilenceFramesInt, sizeof(uint16_t)*m_blockSize);
		MIPAudioMixerBlock block(intervalNumber, pNewFrames);
		m_audioBlocks.push_back(block);
		m_it = m_audioBlocks.end();
		return block;
	}

	if (intervalNumber < (*m_it).getInterval())
	{
		if (m_floatSamples)
		{
			float *pNewFrames = new float [m_blockSize];
			
			memcpy(pNewFrames, m_pSilenceFramesFloat, sizeof(float)*m_blockSize);
			MIPAudioMixerBlock block(intervalNumber, pNewFrames);
			m_it = m_audioBlocks.insert(m_it, block);
			m_it++;
			return block;
		}

		// 16 bit signed		
		
		uint16_t *pNewFrames = new uint16_t [m_blockSize];
			
		memcpy(pNewFrames, m_pSilenceFramesInt, sizeof(uint16_t)*m_blockSize);
		MIPAudioMixerBlock block(intervalNumber, pNewFrames);
		m_it = m_audioBlocks.insert(m_it, block);
		m_it++;
		return block;
	}

	if (intervalNumber == (*m_it).getInterval())
	{
		MIPAudioMixerBlock block = (*m_it);
		m_it++;
		return block;
	}

	// intervalNumber > (*m_it).getInterval()
	
	bool done = false;
	
	while (!done)
	{
		m_it++;
		if (m_it == m_audioBlocks.end())
		{
			done = true;
		}
		else
		{
			if ((*m_it).getInterval() == intervalNumber)
			{
				MIPAudioMixerBlock block = (*m_it);
				m_it++;
				return block;
			}

			if (intervalNumber < (*m_it).getInterval())
			{
				if (m_floatSamples)
				{
					float *pNewFrames = new float [m_blockSize];
			
					memcpy(pNewFrames, m_pSilenceFramesFloat, sizeof(float)*m_blockSize);
					MIPAudioMixerBlock block(intervalNumber, pNewFrames);
					m_it = m_audioBlocks.insert(m_it, block);
					m_it++;
					return block;
				}

				// 16 bit signed
				
				uint16_t *pNewFrames = new uint16_t [m_blockSize];
			
				memcpy(pNewFrames, m_pSilenceFramesInt, sizeof(uint16_t)*m_blockSize);
				MIPAudioMixerBlock block(intervalNumber, pNewFrames);
				m_it = m_audioBlocks.insert(m_it, block);
				m_it++;
				return block;
			}
		}
	} 
	
	if (m_floatSamples)
	{
		float *pNewFrames = new float [m_blockSize];
			
		memcpy(pNewFrames, m_pSilenceFramesFloat, sizeof(float)*m_blockSize);
		MIPAudioMixerBlock block(intervalNumber, pNewFrames);
		m_audioBlocks.push_back(block);
		m_it = m_audioBlocks.end();
		return block;
	}

	// 16 bit signed
	
	uint16_t *pNewFrames = new uint16_t [m_blockSize];
			
	memcpy(pNewFrames, m_pSilenceFramesInt, sizeof(uint16_t)*m_blockSize);
	MIPAudioMixerBlock block(intervalNumber, pNewFrames);
	m_audioBlocks.push_back(block);
	m_it = m_audioBlocks.end();
	return block;
}

bool MIPAudioMixer::processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, 
                                    MIPFeedback *feedback)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_NOTINIT);
		return false;
	}
	if (!feedback->setPlaybackStreamTime(m_playTime))
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_CANTSETPLAYBACKTIME);
		return false;
	}
	feedback->addToPlaybackDelay(m_extraDelay);
	return true;
}

bool MIPAudioMixer::setExtraDelay(MIPTime t)
{
	if (!m_useTimeInfo)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_TIMINGINFOISNOTUSED);
		return false;
	}
	
	if (t.getValue() < 0)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_NEGATIVEDELAY);
		return false;
	}

	if (t.getValue() > 300.0)
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_DELAYTOOLARGE);
		return false;
	}
	
	m_extraDelay = t;
	return true;
}

