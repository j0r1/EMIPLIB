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
#include "mipalawdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#define MIPALAWDECODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPALAWDECODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPALAWDECODER_ERRSTR_BADMESSAGE			"Only a-law encoded audio messages are accepted"

int16_t MIPALawDecoder::m_decompressionTable[256] = 
{
	-5504,-5248,-6016,-5760,-4480,-4224,-4992,-4736,
	-7552,-7296,-8064,-7808,-6528,-6272,-7040,-6784,
	-2752,-2624,-3008,-2880,-2240,-2112,-2496,-2368,
	-3776,-3648,-4032,-3904,-3264,-3136,-3520,-3392,
	-22016,-20992,-24064,-23040,-17920,-16896,-19968,-18944,
	-30208,-29184,-32256,-31232,-26112,-25088,-28160,-27136,
	-11008,-10496,-12032,-11520,-8960,-8448,-9984,-9472,
	-15104,-14592,-16128,-15616,-13056,-12544,-14080,-13568,
	-344,-328,-376,-360,-280,-264,-312,-296,
	-472,-456,-504,-488,-408,-392,-440,-424,
	-88,-72,-120,-104,-24,-8,-56,-40,
	-216,-200,-248,-232,-152,-136,-184,-168,
	-1376,-1312,-1504,-1440,-1120,-1056,-1248,-1184,
	-1888,-1824,-2016,-1952,-1632,-1568,-1760,-1696,
	-688,-656,-752,-720,-560,-528,-624,-592,
	-944,-912,-1008,-976,-816,-784,-880,-848,
	5504,5248,6016,5760,4480,4224,4992,4736,
	7552,7296,8064,7808,6528,6272,7040,6784,
	2752,2624,3008,2880,2240,2112,2496,2368,
	3776,3648,4032,3904,3264,3136,3520,3392,
	22016,20992,24064,23040,17920,16896,19968,18944,
	30208,29184,32256,31232,26112,25088,28160,27136,
	11008,10496,12032,11520,8960,8448,9984,9472,
	15104,14592,16128,15616,13056,12544,14080,13568,
	344,328,376,360,280,264,312,296,
	472,456,504,488,408,392,440,424,
	88,72,120,104,24,8,56,40,
	216,200,248,232,152,136,184,168,
	1376,1312,1504,1440,1120,1056,1248,1184,
	1888,1824,2016,1952,1632,1568,1760,1696,
	688,656,752,720,560,528,624,592,
	944,912,1008,976,816,784,880,848
};

MIPALawDecoder::MIPALawDecoder() : MIPComponent("MIPALawDecoder")
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
		pSamples[i] = m_decompressionTable[(int)pBuffer[i]];
	
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

