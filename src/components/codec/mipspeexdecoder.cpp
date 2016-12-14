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

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "mipspeexdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

#define MIPSPEEXDECODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPSPEEXDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPSPEEXDECODER_ERRSTR_BADMESSAGE				"Bad message"
	
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

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, SpeexStateInfo *>::iterator it;
#else
	hash_map<uint64_t, SpeexStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	for (it = m_speexStates.begin() ; it != m_speexStates.end() ; it++)
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

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, SpeexStateInfo *>::iterator it;
#else
	hash_map<uint64_t, SpeexStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_speexStates.begin();
	while (it != m_speexStates.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
#if defined(WIN32) || defined(_WIN32_WCE)
			hash_map<uint64_t, SpeexStateInfo *>::iterator it2 = it;
#else
			hash_map<uint64_t, SpeexStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it2 = it;
#endif // Win32
			it++;
	
			delete (*it2).second;
			m_speexStates.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
}

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
	if (!(sampRate == 8000 || sampRate == 16000 || sampRate == 32000))
	{
		// ignore message: we don't want malformed messages to stop the chain
		return true;
	}

	if (pEncMsg->getNumberOfChannels() != 1)
	{
		// ignore message: we don't want malformed messages to stop the chain
		return true;
	}	

	uint64_t sourceID = pEncMsg->getSourceID();

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, SpeexStateInfo *>::iterator it;
#else
	hash_map<uint64_t, SpeexStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_speexStates.find(sourceID);
	SpeexStateInfo *pSpeexInf = 0;
	SpeexStateInfo::BandWidth b;

	if (sampRate == 8000)
		b = SpeexStateInfo::NarrowBand;
	else if (sampRate == 16000)
		b = SpeexStateInfo::WideBand;
	else
		b = SpeexStateInfo::UltraWideBand;

	if (it == m_speexStates.end()) // no entry present yet, add one
	{
		pSpeexInf = new SpeexStateInfo(b);
		m_speexStates[sourceID] = pSpeexInf;
	}
	else
		pSpeexInf = (*it).second;

	if (pSpeexInf->getBandWidth() != b)
	{
		// ignore message: we don't want malformed messages to stop the chain
		return true;
	}

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

