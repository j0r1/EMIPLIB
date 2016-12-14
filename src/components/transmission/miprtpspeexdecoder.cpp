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

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "miprtpspeexdecoder.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"
#include "miprtpmessage.h"
#include <rtppacket.h>

#include "mipdebug.h"

MIPRTPSpeexDecoder::MIPRTPSpeexDecoder()
{
}

MIPRTPSpeexDecoder::~MIPRTPSpeexDecoder()
{
}

bool MIPRTPSpeexDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit)
{
	size_t length;
	
	length = (size_t)pRTPPack->GetPayloadLength();
	if (length < 1)
		return false;

	if (timestampUnit < 0) // If available, we'll use the timestamp that's been set in or calculated by jrtplib
	{
		if (m_speexUtil.processBytes(pRTPPack->GetPayloadData(),pRTPPack->GetPayloadLength()))
		{
			m_sampRate = m_speexUtil.getSamplingRate();
			timestampUnit = 1.0/((real_t)m_sampRate);
		}
	}
	
	return true;
}

MIPMediaMessage *MIPRTPSpeexDecoder::createNewMessage(const RTPPacket *pRTPPack)
{
	size_t length = pRTPPack->GetPayloadLength();
	uint8_t *pData = new uint8_t [length];
	
	memcpy(pData, pRTPPack->GetPayloadData(), length);
	MIPEncodedAudioMessage *pEncMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX, m_sampRate, 1, -1, pData, length, true);

	return pEncMsg;
}

#endif // MIPCONFIG_SUPPORT_SPEEX
