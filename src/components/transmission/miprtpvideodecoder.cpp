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
#include "miprtpvideodecoder.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"
#include "miprtpmessage.h"
#include <rtppacket.h>

#include "mipdebug.h"

MIPRTPVideoDecoder::MIPRTPVideoDecoder()
{
}

MIPRTPVideoDecoder::~MIPRTPVideoDecoder()
{
}

bool MIPRTPVideoDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit)
{
	if (!(pRTPPack->GetPayloadType() == 103))
		return false;

	size_t length;
	
	if ((length = (size_t)pRTPPack->GetPayloadLength()) <= 9)
		return false;

	timestampUnit = 1.0/90000.0;
	
	return true;
}

MIPMediaMessage *MIPRTPVideoDecoder::createNewMessage(const RTPPacket *pRTPPack)
{
	size_t length = pRTPPack->GetPayloadLength()-9;
	uint32_t width = 0;
	uint32_t height = 0;
	uint8_t *pData = new uint8_t [length];
	const uint8_t *pPayload = pRTPPack->GetPayloadData();
		
	memcpy(pData,pPayload+9,length);
	width = (uint32_t)pPayload[1];
	width |= ((uint32_t)pPayload[2])<<8;
	width |= ((uint32_t)pPayload[3])<<16;
	width |= ((uint32_t)pPayload[4])<<24;
	height = (uint32_t)pPayload[5];
	height |= ((uint32_t)pPayload[6])<<8;
	height |= ((uint32_t)pPayload[7])<<16;
	height |= ((uint32_t)pPayload[8])<<24;

	if (height < 0 || width < 0 || height > 2048 || width > 2048)
		return 0;
	
	MIPEncodedVideoMessage *pVidMsg = new MIPEncodedVideoMessage(MIPENCODEDVIDEOMESSAGE_TYPE_H263P, (int)width, (int)height, pData, length, true);
		
	return pVidMsg;
}

