/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "mipspeexdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include <speex/speex.h>

#include "mipdebug.h"

#define MIPSPEEXDECODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPSPEEXDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPSPEEXDECODER_ERRSTR_BADMESSAGE				"Bad message"
#define MIPSPEEXDECODER_ERRSTR_BADSAMPRATE				"Bad sampling rate"
	
MIPSpeexDecoder::SpeexStateInfo::SpeexStateInfo(SpeexBandWidth b)
{ 
	m_lastTime = MIPTime::getCurrentTime(); 
	m_pBits = new SpeexBits;
	speex_bits_init(m_pBits); 
	if (b == NarrowBand)
		m_pState = speex_decoder_init(&speex_nb_mode);
	else if (b == WideBand)
		m_pState = speex_decoder_init(&speex_wb_mode);
	else
		m_pState = speex_decoder_init(&speex_uwb_mode);
	m_bandWidth = b;
	speex_decoder_ctl(m_pState, SPEEX_GET_FRAME_SIZE, &m_numFrames); 
}
		
MIPSpeexDecoder::SpeexStateInfo::~SpeexStateInfo() 
{ 
	speex_bits_destroy(m_pBits);
	delete m_pBits;
	speex_decoder_destroy(m_pState);
}


MIPSpeexDecoder::MIPSpeexDecoder() : MIPComponent("MIPSpeexDecoder")
{
	m_init = false;
}

MIPSpeexDecoder::~MIPSpeexDecoder()
{
	destroy();
}

bool MIPSpeexDecoder::init(bool floatSamples)
{
	if (m_init)
	{
		setErrorString(MIPSPEEXDECODER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_lastIteration = -1;
	m_msgIt = m_messages.begin();
	m_lastExpireTime = MIPTime::getCurrentTime();
	m_floatSamples = floatSamples;
	m_init = true;
	return true;
}

bool MIPSpeexDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPSPEEXDECODER_ERRSTR_NOTINIT);
		return false;
	}

	for (auto it = m_speexStates.begin() ; it != m_speexStates.end() ; it++)
		delete (*it).second;
	m_speexStates.clear();

	clearMessages();
	m_init = false;

	return true;
}

void MIPSpeexDecoder::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPSpeexDecoder::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastExpireTime.getValue()) < 60.0)
		return;

	auto it = m_speexStates.begin();
	while (it != m_speexStates.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
			auto it2 = it;
			it++;
	
			delete (*it2).second;
			m_speexStates.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
}

// TODO: for now, we're assuming one frame per packet
// TODO: for now, we're assuming mono sound

bool MIPSpeexDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSPEEXDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX))
	{
		setErrorString(MIPSPEEXDECODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (iteration != m_lastIteration)
	{
		m_lastIteration = iteration;
		clearMessages();
		expire();
	}

	MIPEncodedAudioMessage *pEncMsg = (MIPEncodedAudioMessage *)pMsg;
	int sampRate = pEncMsg->getSamplingRate();
	SpeexStateInfo::SpeexBandWidth bw;

	if (sampRate == 8000)
		bw = SpeexStateInfo::NarrowBand;
	else if (sampRate == 16000)
		bw = SpeexStateInfo::WideBand;
	else if (sampRate == 32000)
		bw = SpeexStateInfo::UltraWideBand;
	else
	{
		setErrorString(MIPSPEEXDECODER_ERRSTR_BADSAMPRATE);
		return false;
	}

	uint64_t sourceID = pEncMsg->getSourceID();

	auto it = m_speexStates.find(sourceID);
	SpeexStateInfo *pSpeexInf = 0;

	if (it == m_speexStates.end()) // no entry present yet, add one
	{
		pSpeexInf = new SpeexStateInfo(bw);
		m_speexStates[sourceID] = pSpeexInf;
	}
	else
		pSpeexInf = (*it).second;

	if (bw != pSpeexInf->getBandWidth())
		return true; // bandwidth changes are not supported, ignore packet
	
	int numFrames = pSpeexInf->getNumberOfFrames();
	speex_bits_read_from(pSpeexInf->getBits(), (char *)pEncMsg->getData(), (int)pEncMsg->getDataLength());

	pSpeexInf->setUpdateTime();

	if (m_floatSamples)
	{
		float *pFrames = new float [numFrames];
		
		speex_decode(pSpeexInf->getState(), pSpeexInf->getBits(), pFrames);
	
		for (int i = 0 ; i < numFrames ; i++)
			pFrames[i] /= (float)32767.0;
		
		MIPRawFloatAudioMessage *pNewMsg = new MIPRawFloatAudioMessage(sampRate, 1, numFrames, pFrames, true);
		pNewMsg->copyMediaInfoFrom(*pEncMsg); // copy source ID and message time
		m_messages.push_back(pNewMsg);
		m_msgIt = m_messages.begin();
	}
	else // use 16 bit signed native encoding
	{
		uint16_t *pFrames = new uint16_t [numFrames];
		
		speex_decode_int(pSpeexInf->getState(), pSpeexInf->getBits(), (int16_t *)pFrames);
		
		MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(sampRate, 1, numFrames, true, MIPRaw16bitAudioMessage::Native, pFrames, true);
		pNewMsg->copyMediaInfoFrom(*pEncMsg); // copy source ID and message time
		m_messages.push_back(pNewMsg);
		m_msgIt = m_messages.begin();
	}
	return true;
}

bool MIPSpeexDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSPEEXDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_lastIteration)
	{
		m_lastIteration = iteration;
		clearMessages();
		expire();
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

#endif // MIPCONFIG_SUPPORT_SPEEX

