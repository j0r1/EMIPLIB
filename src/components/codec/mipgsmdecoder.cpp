/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_GSM

#include "mipgsmdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include "gsm.h"

#include "mipdebug.h"

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

#define MIPGSMDECODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPGSMDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPGSMDECODER_ERRSTR_BADMESSAGE					"Bad message"
#define MIPGSMDECODER_ERRSTR_BADSAMPRATE				"Bad sampling rate"
#define MIPGSMDECODER_ERRSTR_BADENCODEDFRAMESIZE			"The number of bytes in an encoded frame should be 33"
#define MIPGSMDECODER_NUMFRAMES						160
#define MIPGSMDECODER_SAMPRATE						8000
#define MIPGSMDECODER_FRAMESIZE						33
	
MIPGSMDecoder::GSMStateInfo::GSMStateInfo()
{ 
	m_lastTime = MIPTime::getCurrentTime(); 
	m_pState = gsm_create(); // TODO: check if this goes wrong?
}
		
MIPGSMDecoder::GSMStateInfo::~GSMStateInfo() 
{ 
	gsm_destroy(m_pState);
}


MIPGSMDecoder::MIPGSMDecoder() : MIPComponent("MIPGSMDecoder")
{
	m_init = false;
}

MIPGSMDecoder::~MIPGSMDecoder()
{
	destroy();
}

bool MIPGSMDecoder::init()
{
	if (m_init)
	{
		setErrorString(MIPGSMDECODER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_lastIteration = -1;
	m_msgIt = m_messages.begin();
	m_lastExpireTime = MIPTime::getCurrentTime();
	m_init = true;
	return true;
}

bool MIPGSMDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPGSMDECODER_ERRSTR_NOTINIT);
		return false;
	}

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, GSMStateInfo *>::iterator it;
#else
	hash_map<uint64_t, GSMStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	for (it = m_gsmStates.begin() ; it != m_gsmStates.end() ; it++)
		delete (*it).second;
	m_gsmStates.clear();

	clearMessages();
	m_init = false;

	return true;
}

void MIPGSMDecoder::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPGSMDecoder::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastExpireTime.getValue()) < 60.0)
		return;

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, GSMStateInfo *>::iterator it;
#else
	hash_map<uint64_t, GSMStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_gsmStates.begin();
	while (it != m_gsmStates.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
#if defined(WIN32) || defined(_WIN32_WCE)
			hash_map<uint64_t, GSMStateInfo *>::iterator it2 = it;
#else
			hash_map<uint64_t, GSMStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it2 = it;
#endif // Win32
			it++;
	
			delete (*it2).second;
			m_gsmStates.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
}

// TODO: for now, we're assuming one frame per packet

bool MIPGSMDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPGSMDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_GSM))
	{
		setErrorString(MIPGSMDECODER_ERRSTR_BADMESSAGE);
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

	if (sampRate != MIPGSMDECODER_SAMPRATE)
	{
		setErrorString(MIPGSMDECODER_ERRSTR_BADSAMPRATE);
		return false;
	}

	if (pEncMsg->getDataLength() != MIPGSMDECODER_FRAMESIZE)
	{
		setErrorString(MIPGSMDECODER_ERRSTR_BADENCODEDFRAMESIZE);
		return false;
	}

	uint64_t sourceID = pEncMsg->getSourceID();

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, GSMStateInfo *>::iterator it;
#else
	hash_map<uint64_t, GSMStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_gsmStates.find(sourceID);
	GSMStateInfo *pGSMInf = 0;

	if (it == m_gsmStates.end()) // no entry present yet, add one
	{
		pGSMInf = new GSMStateInfo();
		m_gsmStates[sourceID] = pGSMInf;
	}
	else
		pGSMInf = (*it).second;

	pGSMInf->setUpdateTime();

	// use 16 bit signed native encoding
	
	uint16_t *pFrames = new uint16_t [MIPGSMDECODER_NUMFRAMES];
	
	gsm_decode(pGSMInf->getState(), (gsm_byte *)pEncMsg->getData(), (gsm_signal *)pFrames);
	
	MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(MIPGSMDECODER_SAMPRATE, 1, MIPGSMDECODER_NUMFRAMES, true, MIPRaw16bitAudioMessage::Native, pFrames, true);
	pNewMsg->copyMediaInfoFrom(*pEncMsg); // copy source ID and message time
	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPGSMDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPGSMDECODER_ERRSTR_NOTINIT);
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

#endif // MIPCONFIG_SUPPORT_GSM

