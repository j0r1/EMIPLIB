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
#include "miprtph263encoder.h"
#include "miprtpmessage.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"

#include "mipdebug.h"

#define MIPRTPH263ENCODER_ERRSTR_BADMESSAGE		"Can't understand message"
#define MIPRTPH263ENCODER_ERRSTR_CANTENCODE		"Can't encode this message"
#define MIPRTPH263ENCODER_ERRSTR_NOTINIT		"RTP encoder not initialized"
#define MIPRTPH263ENCODER_ERRSTR_BADPAYLOADSIZE		"The maximum RTP payload size should be at least 128"

MIPRTPH263Encoder::MIPRTPH263Encoder() : MIPRTPEncoder("MIPRTPH263Encoder")
{
	m_init = false;
}

MIPRTPH263Encoder::~MIPRTPH263Encoder()
{
	cleanUp();
}

bool MIPRTPH263Encoder::init(real_t frameRate, size_t maxPayloadSize)
{
	if (maxPayloadSize < 128)
	{
		setErrorString(MIPRTPH263ENCODER_ERRSTR_BADPAYLOADSIZE);
		return false;
	}

	if (m_init)
		cleanUp();

	m_msgIt = m_messages.begin();
	m_prevIteration = -1;
	m_frameRate = frameRate;
	m_init = true;
	m_maxPayloadSize = maxPayloadSize;
	return true;
}

bool MIPRTPH263Encoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPH263ENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}
	
	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_ENCODED)
	{
		MIPEncodedVideoMessage *pVidMsg = (MIPEncodedVideoMessage *)pMsg;
		
		// We'll use a timestamp unit of 1.0/90000.0

		real_t tsUnit = 1.0/90000.0;
		uint32_t tsInc = (uint32_t)((1.0/(m_frameRate*tsUnit))+0.5);
		uint8_t payloadType = getPayloadType();
		uint32_t width = (uint32_t)pVidMsg->getWidth();
		uint32_t height = (uint32_t)pVidMsg->getHeight();
		uint8_t typeByte = 0;
		size_t msgSize = pVidMsg->getDataLength();
		
		if (pMsg->getMessageSubtype() != MIPENCODEDVIDEOMESSAGE_TYPE_H263P)
		{
			setErrorString(MIPRTPH263ENCODER_ERRSTR_BADMESSAGE);
			return false;
		}
		
		int extraBytes = 0;
		int offset = 0;

		while (offset < msgSize)
		{
			size_t partSize = msgSize-offset+extraBytes;
			uint32_t partTSInc = 0;
			bool marker = true;

			if (partSize > m_maxPayloadSize)
			{
				partSize = m_maxPayloadSize;
				marker = false;
			}
			else
				partTSInc = tsInc;

			uint8_t *pMsgData = new uint8_t [partSize];
		
			memcpy(pMsgData + extraBytes, pVidMsg->getImageData() + offset, partSize-extraBytes);

			if (extraBytes == 0) // first part of a frame
			{
				// first two bytes should be zeroes, so we can overwrite these with
				// the H.263 header from RFC 4629

				pMsgData[0] = 0x04;
				pMsgData[1] = 0x00;
			}
			else
			{
				// Add H.263 header for this fragment
				pMsgData[0] = 0;
				pMsgData[1] = 0;
			}

			MIPRTPSendMessage *pNewMsg;

			pNewMsg = new MIPRTPSendMessage(pMsgData, partSize, payloadType, marker, partTSInc);
			pNewMsg->setSamplingInstant(pVidMsg->getTime());

			m_messages.push_back(pNewMsg);
		

			offset += (partSize-extraBytes);
			extraBytes = 2;
		}
	
		m_msgIt = m_messages.begin();
	}
	else
	{
		setErrorString(MIPRTPH263ENCODER_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPRTPH263Encoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPH263ENCODER_ERRSTR_NOTINIT);
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

void MIPRTPH263Encoder::cleanUp()
{
	clearMessages();
	m_init = false;
}

void MIPRTPH263Encoder::clearMessages()
{
	std::list<MIPRTPSendMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

