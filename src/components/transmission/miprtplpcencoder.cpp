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

#ifdef MIPCONFIG_SUPPORT_LPC

#include "miprtplpcencoder.h"
#include "miprtpmessage.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"

#include "mipdebug.h"

#define MIPRTPLPCENCODER_ERRSTR_BADMESSAGE			"Can't understand message"
#define MIPRTPLPCENCODER_ERRSTR_NOTINIT				"RTP LPC encoder not initialized"

MIPRTPLPCEncoder::MIPRTPLPCEncoder() : MIPRTPEncoder("MIPRTPLPCEncoder")
{
	m_init = false;
}

MIPRTPLPCEncoder::~MIPRTPLPCEncoder()
{
	cleanUp();
}

bool MIPRTPLPCEncoder::init()
{
	if (m_init)
		cleanUp();

	m_prevIteration = -1;
	m_init = true;
	return true;
}

bool MIPRTPLPCEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPLPCENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_LPC))
	{
		setErrorString(MIPRTPLPCENCODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	MIPEncodedAudioMessage *pEncMsg = (MIPEncodedAudioMessage *)pMsg;
	
	const void *pData = pEncMsg->getData();
	bool marker = false;
	size_t length = pEncMsg->getDataLength();
	uint8_t *pPayload = new uint8_t [length];

	memcpy(pPayload,pData,length);

	MIPRTPSendMessage *pNewMsg;

	pNewMsg = new MIPRTPSendMessage(pPayload, length, getPayloadType(), marker, pEncMsg->getNumberOfFrames());
	pNewMsg->setSamplingInstant(pEncMsg->getTime());
	
	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
		
	return true;
}

bool MIPRTPLPCEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPLPCENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (m_msgIt == m_messages.end())
	{
		m_msgIt = m_messages.begin();
		*pMsg = 0;
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	return true;
}

void MIPRTPLPCEncoder::cleanUp()
{
	clearMessages();
	m_init = false;
}

void MIPRTPLPCEncoder::clearMessages()
{
	std::list<MIPRTPSendMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

#endif // MIPCONFIG_SUPPORT_LPC
