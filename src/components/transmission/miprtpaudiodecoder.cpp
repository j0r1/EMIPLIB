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
#include "miprtpaudiodecoder.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"
#include "miprtpmessage.h"
#include <rtppacket.h>

#include "mipdebug.h"

MIPRTPAudioDecoder::MIPRTPAudioDecoder() : MIPRTPDecoder("MIPRTPAudioDecoder")
{
}

MIPRTPAudioDecoder::~MIPRTPAudioDecoder()
{
}

bool MIPRTPAudioDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit)
{
	if (pRTPPack->GetPayloadType() != 101)
		return false;

	size_t length;
	
	
	length = (size_t)pRTPPack->GetPayloadLength();

	if (length < 1)
		return false;
	
	const uint8_t *pPayload = pRTPPack->GetPayloadData();

	uint8_t infoByte = pPayload[0];
	
	if ((infoByte&0x80) == 0x80) // compressed
	{
		infoByte &= (~0x80);
		if (infoByte == 1)
			m_sampRate = 8000;
		else if (infoByte == 2)
			m_sampRate = 16000;
		else if (infoByte == 3)
			m_sampRate = 32000;
		else
			return false;

		m_encoded = true;
		m_encodingType = MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX;
	}
	else
	{
		if (length <= 3)
			return false;
	
		if (pPayload[1] != 0)
			return false;
		if (pPayload[2] != 0)
			return false;
	
		if (infoByte&(1<<5))
			m_bytesPerSample = 2;
		else
			m_bytesPerSample = 1;

		if (infoByte&(1<<6))
			m_stereo = true;
		else
			m_stereo = false;
	
		infoByte &= 0x0f;
		
		if (infoByte == 0)
			m_sampRate = 4000;
		else if (infoByte == 1)
			m_sampRate = 8000;
		else if (infoByte == 2)
			m_sampRate = 11025;
		else if (infoByte == 3)
			m_sampRate = 22050;
		else if (infoByte == 4)
			m_sampRate = 44100;
		else
			return false;

		m_encoded = false;
	}
	
	timestampUnit = 1.0/((real_t)m_sampRate);
	
	return true;
}

MIPMediaMessage *MIPRTPAudioDecoder::createNewMessage(const RTPPacket *pRTPPack)
{
	if (!m_encoded)
	{
		size_t length = pRTPPack->GetPayloadLength()-3;
		int numFrames = (int)length;
	
		if (m_bytesPerSample == 2 && (length&1) != 0)
			return 0;
	
		MIPAudioMessage *pAudioMsg = 0;
		int channels = 1;

		if (m_stereo)
		{
			channels = 2;
			numFrames /= 2;
		}
		
		if (m_bytesPerSample == 2)
		{
			uint16_t *pFrames = new uint16_t [length / 2];
	
			numFrames /= 2;
			memcpy(pFrames, pRTPPack->GetPayloadData() + 3, length);
			pAudioMsg = new MIPRaw16bitAudioMessage(m_sampRate, channels, numFrames, false, true, pFrames, true);
		}
		else // one byte per sample
		{
			uint8_t *pFrames = new uint8_t [length];
			memcpy(pFrames, pRTPPack->GetPayloadData() + 3, length);
			pAudioMsg = new MIPRawU8AudioMessage(m_sampRate, channels, numFrames, pFrames, true);
		}
		
		return pAudioMsg;
	}

	// encoded
	
	size_t length = pRTPPack->GetPayloadLength()-1;
	uint8_t *pData = new uint8_t [length];
	
	memcpy(pData, pRTPPack->GetPayloadData() + 1, length);
	MIPEncodedAudioMessage *pEncMsg = new MIPEncodedAudioMessage(m_encodingType, m_sampRate, 1, -1, pData, length, true);

	return pEncMsg;
}

