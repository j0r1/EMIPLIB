/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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

int16_t MIPULawDecoder::m_decompressionTable[256] = 
{
	-32124,-31100,-30076,-29052,-28028,-27004,-25980,-24956,
	-23932,-22908,-21884,-20860,-19836,-18812,-17788,-16764,
	-15996,-15484,-14972,-14460,-13948,-13436,-12924,-12412,
	-11900,-11388,-10876,-10364,-9852,-9340,-8828,-8316,
	-7932,-7676,-7420,-7164,-6908,-6652,-6396,-6140,
	-5884,-5628,-5372,-5116,-4860,-4604,-4348,-4092,
	-3900,-3772,-3644,-3516,-3388,-3260,-3132,-3004,
	-2876,-2748,-2620,-2492,-2364,-2236,-2108,-1980,
	-1884,-1820,-1756,-1692,-1628,-1564,-1500,-1436,
	-1372,-1308,-1244,-1180,-1116,-1052,-988,-924,
	-876,-844,-812,-780,-748,-716,-684,-652,
	-620,-588,-556,-524,-492,-460,-428,-396,
	-372,-356,-340,-324,-308,-292,-276,-260,
	-244,-228,-212,-196,-180,-164,-148,-132,
	-120,-112,-104,-96,-88,-80,-72,-64,
	-56,-48,-40,-32,-24,-16,-8,0,
	32124,31100,30076,29052,28028,27004,25980,24956,
	23932,22908,21884,20860,19836,18812,17788,16764,
	15996,15484,14972,14460,13948,13436,12924,12412,
	11900,11388,10876,10364,9852,9340,8828,8316,
	7932,7676,7420,7164,6908,6652,6396,6140,
	5884,5628,5372,5116,4860,4604,4348,4092,
	3900,3772,3644,3516,3388,3260,3132,3004,
	2876,2748,2620,2492,2364,2236,2108,1980,
	1884,1820,1756,1692,1628,1564,1500,1436,
	1372,1308,1244,1180,1116,1052,988,924,
	876,844,812,780,748,716,684,652,
	620,588,556,524,492,460,428,396,
	372,356,340,324,308,292,276,260,
	244,228,212,196,180,164,148,132,
	120,112,104,96,88,80,72,64,
	56,48,40,32,24,16,8,0
};

MIPULawDecoder::MIPULawDecoder() : MIPComponent("MIPULawDecoder")
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
		pSamples[i] = m_decompressionTable[(int)pBuffer[i]];
	
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

