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

#include "miplpcencoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include "lpccodec.h"

#include "mipdebug.h"

#define MIPLPCENCODER_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPLPCENCODER_ERRSTR_NOTINIT						"Not initialized"
#define MIPLPCENCODER_ERRSTR_BADFRAMES						"The number of frames in the raw audio message should be 160"
#define MIPLPCENCODER_ERRSTR_BADMESSAGE						"This component expects raw 16-bit signed native endian messages"
#define MIPLPCENCODER_ERRSTR_BADSAMPRATE					"The sampling rate of the raw audio message should be 8000"
#define MIPLPCENCODER_ERRSTR_NOTMONO						"The component expects mono audio messages"
#define MIPLPCENCODER_ENCODEDFRAMESIZE						14
#define MIPLPCENCODER_NUMFRAMES							160
#define MIPLPCENCODER_SAMPRATE							8000

MIPLPCEncoder::MIPLPCEncoder() : MIPComponent("MIPLPCEncoder")
{
	m_init = false;
}

MIPLPCEncoder::~MIPLPCEncoder()
{
	destroy();
}

bool MIPLPCEncoder::init()
{
	if (m_init)
	{
		setErrorString(MIPLPCENCODER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_pEncoder = new LPCEncoder();
	m_pFrameBuffer = new int [MIPLPCENCODER_NUMFRAMES];

	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	
	m_init = true;
	
	return true;
}

bool MIPLPCEncoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPLPCENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	delete [] m_pFrameBuffer;
	delete m_pEncoder;
	clearMessages();
	m_init = false;

	return true;
}

bool MIPLPCEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPLPCENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 ) )
	{
		setErrorString(MIPLPCENCODER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
	
	if (pAudioMsg->getSamplingRate() != MIPLPCENCODER_SAMPRATE)
	{
		setErrorString(MIPLPCENCODER_ERRSTR_BADSAMPRATE);
		return false;
	}
		
	if (pAudioMsg->getNumberOfChannels() != 1)
	{
		setErrorString(MIPLPCENCODER_ERRSTR_NOTMONO);
		return false;
	}
	
	if (pAudioMsg->getNumberOfFrames() != MIPLPCENCODER_NUMFRAMES)
	{
		setErrorString(MIPLPCENCODER_ERRSTR_BADFRAMES);
		return false;
	}

	uint8_t *pBuffer = new uint8_t[MIPLPCENCODER_ENCODEDFRAMESIZE]; // this should be more than enough memory
	int16_t *pFrames = (int16_t *)pAudioMsg->getFrames();

	for (int i = 0 ; i < MIPLPCENCODER_NUMFRAMES ; i++)
		m_pFrameBuffer[i] = (int)pFrames[i];

	m_pEncoder->Encode(m_pFrameBuffer, pBuffer);
	
	MIPEncodedAudioMessage *pNewMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_LPC, MIPLPCENCODER_SAMPRATE, 1, MIPLPCENCODER_NUMFRAMES, pBuffer, MIPLPCENCODER_ENCODEDFRAMESIZE, true);
	pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
	m_messages.push_back(pNewMsg);
	
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPLPCEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPLPCENCODER_ERRSTR_NOTINIT);
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

void MIPLPCEncoder::clearMessages()
{
	std::list<MIPEncodedAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

#endif // MIPCONFIG_SUPPORT_LPC

