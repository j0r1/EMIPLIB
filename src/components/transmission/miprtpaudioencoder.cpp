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
#include "miprtpaudioencoder.h"
#include "miprtpmessage.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"

#define MIPRTPAUDIOENCODER_ERRSTR_BADMESSAGE		"Can't understand message"
#define MIPRTPAUDIOENCODER_ERRSTR_CANTENCODE		"Can't encode this message"
#define MIPRTPAUDIOENCODER_ERRSTR_NOTINIT		"RTP encoder not initialized"

MIPRTPAudioEncoder::MIPRTPAudioEncoder() : MIPComponent("MIPRTPAudioEncoder")
{
	m_init = false;
}

MIPRTPAudioEncoder::~MIPRTPAudioEncoder()
{
	cleanUp();
}

bool MIPRTPAudioEncoder::init()
{
	if (m_init)
		cleanUp();

	m_prevIteration = -1;
	m_init = true;
	return true;
}

bool MIPRTPAudioEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPAUDIOENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}
	
	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW)
	{
		MIPAudioMessage *pAudioMsg = (MIPAudioMessage *)pMsg;
		
		// TODO
		// For now, I'll just use the JVOIPLIB encoding

		bool allowed = true;
		uint8_t *pPayload;
		const void *pData = 0;
		size_t length = 0;
		bool marker = false;
		uint8_t infoByte = 0;
		uint8_t payloadType = 101;
		int multiplier = 1;
		
		if (pAudioMsg->getNumberOfChannels() == 1 || pAudioMsg->getNumberOfChannels() == 2)
		{
			int rate = pAudioMsg->getSamplingRate();
			
			if (rate == 4000)
				infoByte = 0;
			else if (rate == 8000)
				infoByte = 1;
			else if (rate == 11025)
				infoByte = 2;
			else if (rate == 22050)
				infoByte = 3;
			else if (rate == 44100)
				infoByte = 4;
			else
				allowed = false;
			
			if (allowed)
			{
				if (pAudioMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_U8)
				{
					MIPRawU8AudioMessage *pMsg = (MIPRawU8AudioMessage *)pAudioMsg;
					pData = pMsg->getFrames();
					multiplier = 1;
				}
				else if (pAudioMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_U16BE)
				{
					MIPRaw16bitAudioMessage *pMsg = (MIPRaw16bitAudioMessage *)pAudioMsg;
					pData = pMsg->getFrames();
					infoByte |= (1<<5);
					multiplier = 2;
				}
				else
					allowed = false;

				if (allowed)
				{
					if (pAudioMsg->getNumberOfChannels() == 2)
						{
						infoByte |= (1 << 6);
						multiplier *= 2;
					}
				}
			}
		}
		else
			allowed = false;
		
		if (allowed)
		{
			length = 1 + (size_t)(pAudioMsg->getNumberOfFrames()*multiplier) + 2;
			pPayload = new uint8_t [length];
			pPayload[0] = infoByte;
			pPayload[1] = 0;
			pPayload[2] = 0;

			memcpy(pPayload+3,pData,length-3);

			MIPRTPSendMessage *pNewMsg;

			pNewMsg = new MIPRTPSendMessage(pPayload,length,payloadType,marker,pAudioMsg->getNumberOfFrames());
			pNewMsg->setSamplingInstant(pAudioMsg->getTime());
		
			m_messages.push_back(pNewMsg);
			m_msgIt = m_messages.begin();
		}
		else
		{
			setErrorString(MIPRTPAUDIOENCODER_ERRSTR_CANTENCODE);
			return false;
		}
	}
	else if (pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX)
	{
		MIPEncodedAudioMessage *pEncMsg = (MIPEncodedAudioMessage *)pMsg;
		
		bool allowed = true;
		uint8_t *pPayload;
		const void *pData = pEncMsg->getData();
		size_t length = 0;
		bool marker = false;
		uint8_t infoByte = 0x80;
		uint8_t payloadType = 101;

		int sampRate = pEncMsg->getSamplingRate();

		if (sampRate == 8000)
			infoByte |= 1;
		else if (sampRate == 16000)
			infoByte |= 2;
		else if (sampRate == 32000)
			infoByte |= 3;
		else
			allowed = false;

		if (allowed)
		{
			length = pEncMsg->getDataLength() + 1;
		
			pPayload = new uint8_t [length];
			pPayload[0] = infoByte;

			memcpy(pPayload+1,pData,length-1);

			MIPRTPSendMessage *pNewMsg;

			pNewMsg = new MIPRTPSendMessage(pPayload,length,payloadType,marker,pEncMsg->getNumberOfFrames());
			pNewMsg->setSamplingInstant(pEncMsg->getTime());
		
			m_messages.push_back(pNewMsg);
			m_msgIt = m_messages.begin();
		}
		else
		{
			setErrorString(MIPRTPAUDIOENCODER_ERRSTR_CANTENCODE);
			return false;
		}
	}
	else
	{
		setErrorString(MIPRTPAUDIOENCODER_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPRTPAudioEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPAUDIOENCODER_ERRSTR_NOTINIT);
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

void MIPRTPAudioEncoder::cleanUp()
{
	clearMessages();
	m_init = false;
}

void MIPRTPAudioEncoder::clearMessages()
{
	std::list<MIPRTPSendMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

