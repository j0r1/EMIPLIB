/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_OPUS

#include "miprtpopusdecoder.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"
#include "miprtpmessage.h"
#include <jrtplib3/rtppacket.h>

#include "mipdebug.h"

using namespace jrtplib;

MIPRTPOpusDecoder::MIPRTPOpusDecoder()
{
}

MIPRTPOpusDecoder::~MIPRTPOpusDecoder()
{
}

bool MIPRTPOpusDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit, real_t timestampUnitEstimate)
{
	size_t length;

	length = (size_t)pRTPPack->GetPayloadLength();
	if (length < 1)
		return false;

	timestampUnit = 1.0/48000.0;

	return true;
}

void MIPRTPOpusDecoder::createNewMessages(const RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps)
{
	size_t length = pRTPPack->GetPayloadLength();
	uint8_t *pData = new uint8_t [length];
	
	memcpy(pData, pRTPPack->GetPayloadData(), length);
	MIPEncodedAudioMessage *pEncMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_OPUS, -1, 1, -1, pData, length, true);

	messages.push_back(pEncMsg);
	timestamps.push_back(pRTPPack->GetTimestamp());
}

#endif // MIPCONFIG_SUPPORT_OPUS

