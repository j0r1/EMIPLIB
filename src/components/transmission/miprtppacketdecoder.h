/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Hasselt University - Expertise Centre for
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

/**
 * \file miprtppacketdecoder.h
 */

#ifndef MIPRTPPACKETDECODER_H

#define MIPRTPPACKETDECODER_H

#include "mipconfig.h"
#include "miptypes.h"

class RTPPacket;
class MIPMediaMessage;

/** Abstract base class for RTP packet decoders for a specific kind of payload. */
class MIPRTPPacketDecoder
{
public:
	MIPRTPPacketDecoder()									{ }
	virtual ~MIPRTPPacketDecoder()								{ }

	/** Validates an RTP packet and gives information about the timestamp unit of the packet data.
	 *  This function validates an RTP packet and provides information about the timestamp unit
	 *  of the packet data. It has to be implemented in a derived class. The function should return 
	 *  true if the packet is valid and false otherwise and if possible, the timestamp unit should
	 *  be filled in.
	 */
	virtual bool validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit) = 0;

	/** Creates a new message from an RTP packet.
	 *  This function has to be implemented by a derived class. Based on the validated
	 *  RTP packet \c pRTPPack, an appropriate message should be generated. If something 
	 *  went wrong, the function should return 0.
	 */
	virtual MIPMediaMessage *createNewMessage(const RTPPacket *pRTPPack) = 0;
};

#endif // MIPRTPPACKETDECODER_H

