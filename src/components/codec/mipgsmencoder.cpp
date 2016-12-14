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

#ifdef MIPCONFIG_SUPPORT_GSM

#include "mipgsmencoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include "gsm.h"

#include "mipdebug.h"

#define MIPGSMENCODER_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPGSMENCODER_ERRSTR_NOTINIT						"Not initialized"
#define MIPGSMENCODER_ERRSTR_BADFRAMES						"The number of frames in the raw audio message should be 160"
#define MIPGSMENCODER_ERRSTR_BADMESSAGE						"This component expects raw 16-bit signed native endian messages"
#define MIPGSMENCODER_ERRSTR_BADSAMPRATE					"The sampling rate of the raw audio message should be 8000"
#define MIPGSMENCODER_ERRSTR_CANTCREATESTATE					"Unable to initialize the encoder state"
#define MIPGSMENCODER_ERRSTR_NOTMONO						"The component expects mono audio messages"
#define MIPGSMENCODER_ENCODEDFRAMESIZE						33
#define MIPGSMENCODER_NUMFRAMES							160
#define MIPGSMENCODER_SAMPRATE							8000

MIPGSMEncoder::MIPGSMEncoder() : MIPComponent("MIPGSMEncoder")
{
	m_init = false;
}

MIPGSMEncoder::~MIPGSMEncoder()
{
	destroy();
}

bool MIPGSMEncoder::init()
{
	if (m_init)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_pState = gsm_create();
	if (m_pState == 0)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_CANTCREATESTATE);
		return false;
	}

	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	
	m_init = true;
	
	return true;
}

bool MIPGSMEncoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	gsm_destroy(m_pState);
	clearMessages();
	m_init = false;

	return true;
}

bool MIPGSMEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 ) )
	{
		setErrorString(MIPGSMENCODER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
	
	if (pAudioMsg->getSamplingRate() != MIPGSMENCODER_SAMPRATE)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_BADSAMPRATE);
		return false;
	}
		
	if (pAudioMsg->getNumberOfChannels() != 1)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_NOTMONO);
		return false;
	}
	
	if (pAudioMsg->getNumberOfFrames() != MIPGSMENCODER_NUMFRAMES)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_BADFRAMES);
		return false;
	}

	uint8_t *pBuffer = new uint8_t[MIPGSMENCODER_ENCODEDFRAMESIZE]; // this should be more than enough memory

	gsm_encode(m_pState, (gsm_signal *)pAudioMsg->getFrames(), pBuffer);
	
	MIPEncodedAudioMessage *pNewMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_GSM, MIPGSMENCODER_SAMPRATE, 1, MIPGSMENCODER_NUMFRAMES, pBuffer, MIPGSMENCODER_ENCODEDFRAMESIZE, true);
	pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
	m_messages.push_back(pNewMsg);
	
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPGSMEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPGSMENCODER_ERRSTR_NOTINIT);
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

void MIPGSMEncoder::clearMessages()
{
	std::list<MIPEncodedAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

#endif // MIPCONFIG_SUPPORT_GSM

