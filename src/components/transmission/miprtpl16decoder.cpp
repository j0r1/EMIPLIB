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
#include "miprtpl16decoder.h"
#include "miprawaudiomessage.h"
#include "miprtpmessage.h"
#include <jrtplib3/rtppacket.h>

#include "mipdebug.h"

using namespace jrtplib;

MIPRTPL16Decoder::MIPRTPL16Decoder(bool stereo, int sampRate)
{
	if (stereo)
		m_channels = 2;
	else
		m_channels = 1;
	m_sampRate = sampRate;
}

MIPRTPL16Decoder::~MIPRTPL16Decoder()
{
}

bool MIPRTPL16Decoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit, real_t timestampUnitEstimate)
{
	size_t length;
	
	length = (size_t)pRTPPack->GetPayloadLength();
	if (length < 1)
		return false;

	if (m_channels == 1)
	{
		if ((length % 2) != 0)
			return false;
	}
	else // stereo
	{
		if ((length % 4) != 0)
			return false;
	}
	
	timestampUnit = 1.0/(double)m_sampRate;
	return true;
}

void MIPRTPL16Decoder::createNewMessages(const RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps)
{
	size_t length = pRTPPack->GetPayloadLength();
	size_t numSamples = length/2; // 2 bytes per sample

	uint16_t *pFrames = new uint16_t [numSamples];

	memcpy(pFrames, pRTPPack->GetPayloadData(), length);

	MIPRaw16bitAudioMessage *pRawMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_channels, numSamples/m_channels, true, MIPRaw16bitAudioMessage::BigEndian, pFrames, true);

	messages.push_back(pRawMsg);
	timestamps.push_back(pRTPPack->GetTimestamp());
}

