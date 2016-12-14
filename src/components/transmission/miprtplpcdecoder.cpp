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
#include "miprtplpcdecoder.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"
#include "miprtpmessage.h"
#include <rtppacket.h>

#include "mipdebug.h"

#define MIPRTPLPCDECODER_SIZE 						14
#define MIPRTPLPCDECODER_NUMFRAMES					160

MIPRTPLPCDecoder::MIPRTPLPCDecoder()
{
}

MIPRTPLPCDecoder::~MIPRTPLPCDecoder()
{
}

bool MIPRTPLPCDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit)
{
	size_t length;
	
	length = (size_t)pRTPPack->GetPayloadLength();
	if (length != MIPRTPLPCDECODER_SIZE) // TODO: for now we only support one LPC frame per packet
		return false;
	
	timestampUnit = 1.0/8000.0;
	return true;
}

MIPMediaMessage *MIPRTPLPCDecoder::createNewMessage(const RTPPacket *pRTPPack)
{
	size_t length = pRTPPack->GetPayloadLength();
	uint8_t *pData = new uint8_t [length];
	
	memcpy(pData, pRTPPack->GetPayloadData(), length);
	MIPEncodedAudioMessage *pEncMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_LPC, 8000, 1, MIPRTPLPCDECODER_NUMFRAMES, pData, length, true);

	return pEncMsg;
}

