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
#include "mipaudiofilter.h"
#include <math.h>

#include "mipdebug.h"

#define MIPAUDIOFILTER_ERRSTR_NOTINIT				"Not initialized"
#define MIPAUDIOFILTER_ERRSTR_BADMESSAGETYPE			"Component requires floating point raw audio messages"
#define MIPAUDIOFILTER_ERRSTR_BADNUMCHANNELS			"Received an audio message containing a wrong number of channels"
#define MIPAUDIOFILTER_ERRSTR_BADNUMFRAMES			"Received an audio message containing a wrong amount of samples"

MIPAudioFilter::MIPAudioFilter() : MIPComponent("MIPAudioFilter")
{
	m_init = false;
}

MIPAudioFilter::~MIPAudioFilter()
{
	cleanUp();
}

bool MIPAudioFilter::init(int sampRate, int channels, MIPTime interval)
{
	if (m_init)
		cleanUp();

	m_useLow = false;
	m_useHigh = false;
	m_useMiddle = false;
	m_sampRate = sampRate; // TODO: perform check
	m_channels = channels; // TODO: perform check

	m_numFrames = (int)(interval.getValue()*(real_t)sampRate+0.5);
	m_audioSize = m_numFrames*channels;
	
	m_pAM = new float[m_numFrames*m_numFrames];
	m_pBM = new float[m_numFrames*m_numFrames];
	m_pA = new float[m_numFrames];
	m_pB = new float[m_numFrames];

#define MIPAUDIOFILTER_PI 3.14159265358979323844

	int pos = 0;
	for (int n = 0 ; n < m_numFrames ; n++)
	{
		for (int i = 0 ; i < m_numFrames ; i++)
		{
			m_pAM[i+n*m_numFrames] = (float)cosl(((real_t)(n*i))*MIPAUDIOFILTER_PI/(real_t)m_numFrames);
			m_pBM[i+n*m_numFrames] = (float)sinl(((real_t)(n*i))*MIPAUDIOFILTER_PI/(real_t)m_numFrames);
		}
	}
	
	m_pBuf = new float[m_numFrames];
	m_period = (float)(interval.getValue());
	
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_init = true;
	
	return true;
}

bool MIPAudioFilter::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOFILTER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
	{
		setErrorString(MIPAUDIOFILTER_ERRSTR_BADMESSAGETYPE);
		return false;
	}

	MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;

	int numFrames = pAudioMsg->getNumberOfFrames();
	int numChannels = pAudioMsg->getNumberOfChannels();

	if (numFrames != m_numFrames)
	{
		setErrorString(MIPAUDIOFILTER_ERRSTR_BADNUMFRAMES);
		return false;
	}

	if (numChannels != m_channels)
	{
		setErrorString(MIPAUDIOFILTER_ERRSTR_BADNUMCHANNELS);
		return false;
	}
	
	const float *pSamplesFloatIn = pAudioMsg->getFrames();
	float *pSamplesFloat = new float[m_audioSize];

	for (int channel = 0 ; channel < m_channels ; channel++)
	{
		int pos = channel;
		for (int i = 0 ; i < m_numFrames ; i++, pos += m_channels)
			m_pBuf[i] = pSamplesFloatIn[pos];
		
		for (int n = 0 ; n < m_numFrames ; n++)
		{
			float a = 0;
			float b = 0;
			for (int i = 0 ; i < m_numFrames ; i++)
			{
				a += m_pAM[i+n*m_numFrames]*m_pBuf[i];
				b += m_pBM[i+n*m_numFrames]*m_pBuf[i];
			}
			m_pA[n] = a;
			m_pB[n] = b;
		}

	
		if (m_useLow)
		{
			int idx = (int)(m_period*m_lowFreq+0.5f);

			if (idx > numFrames-1)
				idx = numFrames-1;

			for (int n = 0 ; n <= idx ; n++)
			{
				m_pA[n] = 0;
				m_pB[n] = 0;
			}
		}
		
		if (m_useHigh)
		{
			int idx = (int)(m_period*m_highFreq+0.5f);

			if (idx < 0)
				idx = 0;

			for (int n = idx ; n < numFrames ; n++)
			{
				m_pA[n] = 0;
				m_pB[n] = 0;
			}
		}

		if (m_useMiddle)
		{
			int idx1 = (int)(m_period*m_midLowFreq+0.5f);
			int idx2 = (int)(m_period*m_midHighFreq+0.5f);

			if (idx1 < 0)
				idx1 = 0;
			if (idx2 > numFrames-1)
				idx2 = numFrames-1;

			for (int n = idx1 ; n <= idx2 ; n++)
			{
				m_pA[n] = 0;
				m_pB[n] = 0;
			}
		}

		pos = channel;
		for (int i = 0 ; i < m_numFrames ; i++, pos += m_channels)
		{
			float f1 = 0;
			float f2 = 0;
			for (int n = 0 ; n < m_numFrames ; n++)
			{
				f1 += m_pAM[n+i*m_numFrames]*m_pA[n];
				f2 += m_pBM[n+i*m_numFrames]*m_pB[n];
			}
			pSamplesFloat[pos] = ((f1+f2)/(float)m_numFrames);
		}
	}

	MIPAudioMessage *pNewMsg = new MIPRawFloatAudioMessage(0,0,0,pSamplesFloat,true);
	pNewMsg->copyAudioInfoFrom(*pAudioMsg);

	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPAudioFilter::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOFILTER_ERRSTR_NOTINIT);
		return false;
	}

	if (m_prevIteration != iteration)
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

void MIPAudioFilter::cleanUp()
{
	if (!m_init)
		return;

	delete [] m_pAM;
	delete [] m_pBM;
	delete [] m_pA;
	delete [] m_pB;
	delete [] m_pBuf;
	clearMessages();
	
	m_init = false;
}

void MIPAudioFilter::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

