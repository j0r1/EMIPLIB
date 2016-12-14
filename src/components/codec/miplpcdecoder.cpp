/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_LPC

#include "miplpcdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include "lpccodec.h"

#include "mipdebug.h"

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

#define MIPLPCDECODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPLPCDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPLPCDECODER_ERRSTR_BADMESSAGE					"Bad message"
#define MIPLPCDECODER_ERRSTR_BADSAMPRATE				"Bad sampling rate"
#define MIPLPCDECODER_ERRSTR_BADENCODEDFRAMESIZE			"The number of bytes in an encoded frame should be 33"
#define MIPLPCDECODER_NUMFRAMES						160
#define MIPLPCDECODER_SAMPRATE						8000
#define MIPLPCDECODER_FRAMESIZE						14
	
MIPLPCDecoder::LPCStateInfo::LPCStateInfo()
{ 
	m_lastTime = MIPTime::getCurrentTime(); 
	m_pDecoder = new LPCDecoder();
}
		
MIPLPCDecoder::LPCStateInfo::~LPCStateInfo() 
{ 
	delete m_pDecoder;
}


MIPLPCDecoder::MIPLPCDecoder() : MIPComponent("MIPLPCDecoder")
{
	m_init = false;
}

MIPLPCDecoder::~MIPLPCDecoder()
{
	destroy();
}

bool MIPLPCDecoder::init()
{
	if (m_init)
	{
		setErrorString(MIPLPCDECODER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_lastIteration = -1;
	m_msgIt = m_messages.begin();
	m_lastExpireTime = MIPTime::getCurrentTime();
	m_pFrameBuffer = new int [MIPLPCDECODER_NUMFRAMES];
	m_init = true;
	return true;
}

bool MIPLPCDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPLPCDECODER_ERRSTR_NOTINIT);
		return false;
	}

	delete [] m_pFrameBuffer;

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, LPCStateInfo *>::iterator it;
#else
	hash_map<uint64_t, LPCStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	for (it = m_lpcStates.begin() ; it != m_lpcStates.end() ; it++)
		delete (*it).second;
	m_lpcStates.clear();

	clearMessages();
	m_init = false;

	return true;
}

void MIPLPCDecoder::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPLPCDecoder::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastExpireTime.getValue()) < 60.0)
		return;

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, LPCStateInfo *>::iterator it;
#else
	hash_map<uint64_t, LPCStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_lpcStates.begin();
	while (it != m_lpcStates.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
#if defined(WIN32) || defined(_WIN32_WCE)
			hash_map<uint64_t, LPCStateInfo *>::iterator it2 = it;
#else
			hash_map<uint64_t, LPCStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it2 = it;
#endif // Win32
			it++;
	
			delete (*it2).second;
			m_lpcStates.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
}

// TODO: for now, we're assuming one frame per packet

bool MIPLPCDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPLPCDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_LPC))
	{
		setErrorString(MIPLPCDECODER_ERRSTR_BADMESSAGE);
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

	if (sampRate != MIPLPCDECODER_SAMPRATE)
	{
		setErrorString(MIPLPCDECODER_ERRSTR_BADSAMPRATE);
		return false;
	}

	if (pEncMsg->getDataLength() != MIPLPCDECODER_FRAMESIZE)
	{
		setErrorString(MIPLPCDECODER_ERRSTR_BADENCODEDFRAMESIZE);
		return false;
	}

	uint64_t sourceID = pEncMsg->getSourceID();

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, LPCStateInfo *>::iterator it;
#else
	hash_map<uint64_t, LPCStateInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_lpcStates.find(sourceID);
	LPCStateInfo *pLPCInf = 0;

	if (it == m_lpcStates.end()) // no entry present yet, add one
	{
		pLPCInf = new LPCStateInfo();
		m_lpcStates[sourceID] = pLPCInf;
	}
	else
		pLPCInf = (*it).second;

	pLPCInf->setUpdateTime();

	// use 16 bit signed native encoding
	
	uint16_t *pFrames = new uint16_t [MIPLPCDECODER_NUMFRAMES];
	int16_t *pFrames2 = (int16_t *)pFrames;
	
	pLPCInf->getDecoder()->Decode((unsigned char *)pEncMsg->getData(), m_pFrameBuffer);
	
	for (int i = 0 ; i < MIPLPCDECODER_NUMFRAMES ; i++)
		pFrames2[i] = (int16_t)m_pFrameBuffer[i];
	
	MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(MIPLPCDECODER_SAMPRATE, 1, MIPLPCDECODER_NUMFRAMES, true, MIPRaw16bitAudioMessage::Native, pFrames, true);
	pNewMsg->copyMediaInfoFrom(*pEncMsg); // copy source ID and message time
	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPLPCDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPLPCDECODER_ERRSTR_NOTINIT);
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

#endif // MIPCONFIG_SUPPORT_LPC

