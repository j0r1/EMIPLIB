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
#include "miprtpvideoencoder.h"
#include "miprtpmessage.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"

#include "mipdebug.h"

#define MIPRTPVIDEOENCODER_ERRSTR_BADMESSAGE		"Message type or subtype doesn't correspond to the settings during initialization"
#define MIPRTPVIDEOENCODER_ERRSTR_NOTINIT		"RTP encoder not initialized"
#define MIPRTPVIDEOENCODER_ERRSTR_BADMAXPACKSIZE	"Maximum RTP payload size should be at least 128"
#define MIPRTPVIDEOENCODER_ERRSTR_BADENCODINGTYPE	"Specified encoding type is not valid"

MIPRTPVideoEncoder::MIPRTPVideoEncoder() : MIPRTPEncoder("MIPRTPVideoEncoder")
{
	m_init = false;
}

MIPRTPVideoEncoder::~MIPRTPVideoEncoder()
{
	cleanUp();
}

bool MIPRTPVideoEncoder::init(real_t frameRate, size_t maxPayloadSize, MIPRTPVideoEncoder::EncodingType encType)
{
	if (maxPayloadSize < 128)
	{
		setErrorString(MIPRTPVIDEOENCODER_ERRSTR_BADMAXPACKSIZE);
		return false;
	}

	uint8_t encodingType = 0;
	uint32_t encMsgType = 0;
	uint32_t encSubMsgType = 0;

	switch(encType)
	{
	case MIPRTPVideoEncoder::YUV420P:
		encodingType = 0;
		encMsgType = MIPMESSAGE_TYPE_VIDEO_RAW;
		encSubMsgType = MIPRAWVIDEOMESSAGE_TYPE_YUV420P;
		break;
	case MIPRTPVideoEncoder::H263:
		encodingType = 1;
		encMsgType = MIPMESSAGE_TYPE_VIDEO_ENCODED;
		encSubMsgType = MIPENCODEDVIDEOMESSAGE_TYPE_H263P;
		break;
	default:
		setErrorString(MIPRTPVIDEOENCODER_ERRSTR_BADENCODINGTYPE);
		return false;
	}

	if (m_init)
		cleanUp();

	m_maxPayloadSize = maxPayloadSize;
	m_encodingType = encodingType;
	m_encMsgType = encMsgType;
	m_encSubMsgType = encSubMsgType;

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
	
	if (pMsg->getMessageType() == m_encMsgType && pMsg->getMessageSubtype() == m_encSubMsgType)
	{
		const uint8_t *pVideoData = 0;
		size_t msgSize = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		MIPTime sampTime;

		if (pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_ENCODED)
		{
			MIPEncodedVideoMessage *pVidMsg = (MIPEncodedVideoMessage *)pMsg;
	
			msgSize = pVidMsg->getDataLength();
			width = (uint32_t)pVidMsg->getWidth();
			height = (uint32_t)pVidMsg->getHeight();
			pVideoData = pVidMsg->getImageData();
			sampTime = pVidMsg->getTime();
		}
		else // raw data, should be YUV420P for now
		{
			MIPRawYUV420PVideoMessage *pVidMsg = (MIPRawYUV420PVideoMessage *)pMsg;
	
			width = (uint32_t)pVidMsg->getWidth();
			height = (uint32_t)pVidMsg->getHeight();
			msgSize = (width*height*3)/2;
			pVideoData = pVidMsg->getImageData();
			sampTime = pVidMsg->getTime();
		}

		// We'll use a timestamp unit of 1.0/90000.0

		real_t tsUnit = 1.0/90000.0;
		uint32_t tsInc = (uint32_t)((1.0/(m_frameRate*tsUnit))+0.5);
		uint8_t payloadType = getPayloadType();

		int extraBytes = 9;
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
		
			memcpy(pMsgData + extraBytes, pVideoData + offset, partSize-extraBytes);

			if (extraBytes == 9) // first part of a frame
			{
				pMsgData[0] = m_encodingType;
				pMsgData[1] = (uint8_t)(width&0xff);
				pMsgData[2] = (uint8_t)((width>>8)&0xff);
				pMsgData[3] = (uint8_t)((width>>16)&0xff);
				pMsgData[4] = (uint8_t)((width>>24)&0xff);
				pMsgData[5] = (uint8_t)(height&0xff);
				pMsgData[6] = (uint8_t)((height>>8)&0xff);
				pMsgData[7] = (uint8_t)((height>>16)&0xff);
				pMsgData[8] = (uint8_t)((height>>24)&0xff);
			}
			else
			{
				pMsgData[0] = 0xff; // indicate that it's not the first packet of a frame
			}

			MIPRTPSendMessage *pNewMsg;

			pNewMsg = new MIPRTPSendMessage(pMsgData, partSize, payloadType, marker, partTSInc);
			pNewMsg->setSamplingInstant(sampTime);

			m_messages.push_back(pNewMsg);

			offset += (partSize-extraBytes);
			extraBytes = 1;
		}
	
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

