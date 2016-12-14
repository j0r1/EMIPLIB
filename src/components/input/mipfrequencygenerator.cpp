/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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
#include "mipfrequencygenerator.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include <cmath>

#define MIPFREQUENCYGENERATOR_ERRSTR_NOTINIT		"Not initialized"
#define MIPFREQUENCYGENERATOR_ERRSTR_BADMESSAGE 	"Bad message"

MIPFrequencyGenerator::MIPFrequencyGenerator() : MIPComponent("MIPFrequencyGenerator")
{
	m_pFrames = 0;
	m_numFrames = 0;
	m_pAudioMsg = 0;
	m_gotMessage = false;
}

MIPFrequencyGenerator::~MIPFrequencyGenerator()
{
	cleanUp();
}

bool MIPFrequencyGenerator::init(real_t leftFreq, real_t rightFreq, real_t leftAmp, real_t rightAmp, int sampRate, MIPTime interval)
{
	cleanUp();

	m_leftFreq = leftFreq;
	m_rightFreq = rightFreq;
	m_leftAmp = leftAmp;
	m_rightAmp = rightAmp;

	m_numFrames = (int)((((real_t)sampRate)*interval.getValue())+0.5);
	m_pFrames = new float [m_numFrames*2];
	m_pAudioMsg = new MIPRawFloatAudioMessage(sampRate, 2, m_numFrames, m_pFrames, true);
	m_curTime = 0;
	m_timePerSample = (1.0/(real_t)sampRate);

	m_gotMessage = false;
	return true;
}

bool MIPFrequencyGenerator::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_ISTIME))
	{
		setErrorString(MIPFREQUENCYGENERATOR_ERRSTR_BADMESSAGE);
		return false;
	}
	
	int pos = 0;
	for (int i = 0 ; i < m_numFrames ; i++, pos += 2, m_curTime += m_timePerSample)
	{
		m_pFrames[pos] = (float)(m_leftAmp*cos(2.0*3.1415*m_leftFreq*m_curTime));
		m_pFrames[pos+1] = (float)(m_rightAmp*cos(2.0*3.1415*m_rightFreq*m_curTime));
	}

	m_gotMessage = false;

	return true;
}

bool MIPFrequencyGenerator::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pAudioMsg == 0)
	{
		setErrorString(MIPFREQUENCYGENERATOR_ERRSTR_NOTINIT);
		return false;
	}

	if (!m_gotMessage)
	{
		*pMsg = m_pAudioMsg;
		m_gotMessage = true;
	}
	else
	{
		*pMsg = 0;
		m_gotMessage = false;
	}
	return true;
}

void MIPFrequencyGenerator::cleanUp()
{
	if (m_pAudioMsg)
	{
		delete m_pAudioMsg;
		m_pAudioMsg = 0;
	}
}

