/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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
#include "miprtpvideoencoder.h"
#include "miprtpmessage.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"

#define MIPRTPVIDEOENCODER_ERRSTR_BADMESSAGE		"Can't understand message"
#define MIPRTPVIDEOENCODER_ERRSTR_CANTENCODE		"Can't encode this message"
#define MIPRTPVIDEOENCODER_ERRSTR_NOTINIT		"RTP encoder not initialized"

MIPRTPVideoEncoder::MIPRTPVideoEncoder() : MIPRTPEncoder("MIPRTPVideoEncoder")
{
	m_init = false;
}

MIPRTPVideoEncoder::~MIPRTPVideoEncoder()
{
	cleanUp();
}

bool MIPRTPVideoEncoder::init(real_t frameRate)
{
	if (m_init)
		cleanUp();

	m_msgIt = m_messages.begin();
	m_prevIteration = -1;
	m_frameRate = frameRate;
	m_init = true;
	return true;
}

bool MIPRTPVideoEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPVIDEOENCODER_ERRSTR_NOTINIT);
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
		bool marker = false;
		
		if (pMsg->getMessageSubtype() == MIPENCODEDVIDEOMESSAGE_TYPE_H263P)
		{
			typeByte = 1;
			marker = false;
		}
		else
		{
			setErrorString(MIPRTPVIDEOENCODER_ERRSTR_BADMESSAGE);
			return false;
		}
		
		msgSize += 1 + 2*sizeof(uint32_t);
		uint8_t *pMsgData = new uint8_t [msgSize];
		
		pMsgData[0] = typeByte;
		pMsgData[1] = (uint8_t)(width&0xff);
		pMsgData[2] = (uint8_t)((width>>8)&0xff);
		pMsgData[3] = (uint8_t)((width>>16)&0xff);
		pMsgData[4] = (uint8_t)((width>>24)&0xff);
		pMsgData[5] = (uint8_t)(height&0xff);
		pMsgData[6] = (uint8_t)((height>>8)&0xff);
		pMsgData[7] = (uint8_t)((height>>16)&0xff);
		pMsgData[8] = (uint8_t)((height>>24)&0xff);

		memcpy(pMsgData+9,pVidMsg->getImageData(),msgSize-9);

		MIPRTPSendMessage *pNewMsg;

		pNewMsg = new MIPRTPSendMessage(pMsgData,msgSize,payloadType,marker,tsInc);
		pNewMsg->setSamplingInstant(pVidMsg->getTime());
		
		m_messages.push_back(pNewMsg);
		m_msgIt = m_messages.begin();
	}
	else
	{
		setErrorString(MIPRTPVIDEOENCODER_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPRTPVideoEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPVIDEOENCODER_ERRSTR_NOTINIT);
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

void MIPRTPVideoEncoder::cleanUp()
{
	clearMessages();
	m_init = false;
}

void MIPRTPVideoEncoder::clearMessages()
{
	std::list<MIPRTPSendMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

