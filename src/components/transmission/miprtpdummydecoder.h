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

/**
 * \file miprtpdummydecoder.h
 */

#ifndef MIPRTPDUMMYDECODER_H

#define MIPRTPDUMMYDECODER_H

#include "mipconfig.h"
#include "miprtppacketdecoder.h"

/** Dummy RTP packet decoder.
 *  Dummy RTP packet decoder. If an RTP packet is received with an unsupported
 *  payload type, the MIPRTPDecoder::push function will return an error and the
 *  background thread of the component chain will stop. To avoid this from
 *  happening, a dummy RTP packet decoder can be installed as default packet
 *  decoder.
 */
class MIPRTPDummyDecoder : public MIPRTPPacketDecoder
{
public:
	MIPRTPDummyDecoder()											{ }
	~MIPRTPDummyDecoder()											{ }
private:
	bool validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit)					{ return false; }
	MIPMediaMessage *createNewMessage(const RTPPacket *pRTPPack)						{ return 0; }
};

#endif // MIPRTPDUMMYDECODER_H

