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

#ifdef MIPCONFIG_SUPPORT_SILK

#include "miprtpsilkdecoder.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"
#include "miprtpmessage.h"
#include <jrtplib3/rtppacket.h>
#include <cmath>

#include "mipdebug.h"

using namespace jrtplib;

MIPRTPSILKDecoder::MIPRTPSILKDecoder()
{
}

MIPRTPSILKDecoder::~MIPRTPSILKDecoder()
{
}

bool MIPRTPSILKDecoder::isCloseTo(real_t estimate, real_t value)
{
	if (std::abs(estimate-value) < value/10.0)
		return true;
	return false;
}

bool MIPRTPSILKDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit, real_t timestampUnitEstimate)
{
	size_t length;

	length = (size_t)pRTPPack->GetPayloadLength();
	if (length < 1)
		return false;

	m_sampRate = -1;

	if (timestampUnit < 0)
	{
		if (timestampUnitEstimate > 0)
		{
			int sampRateEstimate = (int)(1.0/timestampUnitEstimate + 0.5);

//			std::cout << sampRateEstimate << std::endl;

			if (isCloseTo(sampRateEstimate, 8000))
				timestampUnit = 1.0/8000.0;
			else if (isCloseTo(sampRateEstimate, 12000))
				timestampUnit = 1.0/12000.0;
			else if (isCloseTo(sampRateEstimate, 16000))
				timestampUnit = 1.0/16000.0;
			else if (isCloseTo(sampRateEstimate, 24000))
				timestampUnit = 1.0/24000.0;
			
			if (timestampUnit > 0)
				m_sampRate = (int)((1.0/timestampUnit)+0.5);

//			std::cout << "Using sampling rate " << m_sampRate << std::endl;
		}
	}
	else
		m_sampRate = (int)((1.0/timestampUnit)+0.5);
	
	return true;
}

void MIPRTPSILKDecoder::createNewMessages(const RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps)
{
	size_t length = pRTPPack->GetPayloadLength();
	uint8_t *pData = new uint8_t [length];
	
	memcpy(pData, pRTPPack->GetPayloadData(), length);
	MIPEncodedAudioMessage *pEncMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_SILK, -1, 1, -1, pData, length, true);

	messages.push_back(pEncMsg);
	timestamps.push_back(pRTPPack->GetTimestamp());
}

#endif // MIPCONFIG_SUPPORT_SILK

