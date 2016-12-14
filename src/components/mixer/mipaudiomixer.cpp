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

bool MIPAudioMixer::init(int sampRate, int channels, MIPTime blockTime, bool useTimeInfo)
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

	m_pSilenceFrames = new float [m_blockSize];
	for (size_t i = 0 ; i < m_blockSize ; i++)
		m_pSilenceFrames[i] = 0;
	m_pMsg = new MIPRawFloatAudioMessage(sampRate, channels, (int)m_blockFrames, m_pSilenceFrames, false);
	m_pBlockFrames = 0;

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
	delete m_pMsg;
	if (m_pBlockFrames)
		delete [] m_pBlockFrames;
	delete [] m_pSilenceFrames;

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
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
	{
		setErrorString(MIPAUDIOMIXER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;

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
	const float *pSamples = pAudioMsg->getFrames();

	initBlockSearch();
	while (numSamplesLeft != 0)
	{
		MIPAudioMixerBlock block = searchBlock(intervalNumber);
		float *blockSamples = block.getFrames();
		
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
	
	return true;
}

bool MIPAudioMixer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_prevIteration != 	iteration)
	{
		m_prevIteration = iteration;
		
		// delete old frames
		delete [] m_pBlockFrames;
		m_pBlockFrames = 0;
		
		if (m_audioBlocks.empty())
			m_pMsg->setFrames(m_pSilenceFrames,false);
		else
		{
			MIPAudioMixerBlock block = *(m_audioBlocks.begin());

			if (block.getInterval() == m_curInterval)
			{
				m_audioBlocks.pop_front();
				m_pBlockFrames = block.getFrames();
				m_pMsg->setFrames(m_pBlockFrames,false);
			}
			else
				m_pMsg->setFrames(m_pSilenceFrames,false);
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
		*pMsg = m_pMsg;
	}
	return true;
}

void MIPAudioMixer::clearAudioBlocks()
{
	std::list<MIPAudioMixerBlock>::iterator it;

	for (it = m_audioBlocks.begin() ; it != m_audioBlocks.end() ; it++)
		delete [] (*it).getFrames();
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
		float *pNewFrames = new float [m_blockSize];
		
		memcpy(pNewFrames, m_pSilenceFrames, sizeof(float)*m_blockSize);
		MIPAudioMixerBlock block(intervalNumber, pNewFrames);
		m_audioBlocks.push_back(block);
		m_it = m_audioBlocks.end();
		return block;
	}

	if (intervalNumber < (*m_it).getInterval())
	{
		float *pNewFrames = new float [m_blockSize];
		
		memcpy(pNewFrames, m_pSilenceFrames, sizeof(float)*m_blockSize);
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
				float *pNewFrames = new float [m_blockSize];
		
				memcpy(pNewFrames, m_pSilenceFrames, sizeof(float)*m_blockSize);
				MIPAudioMixerBlock block(intervalNumber, pNewFrames);
				m_it = m_audioBlocks.insert(m_it, block);
				m_it++;
				return block;
			}
		}
	} 
	
	float *pNewFrames = new float [m_blockSize];
		
	memcpy(pNewFrames, m_pSilenceFrames, sizeof(float)*m_blockSize);
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

