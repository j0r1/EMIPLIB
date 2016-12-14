/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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
 * \file miprtpspeexdecoder.h
 */

#ifndef MIPRTPSPEEXDECODER_H

#define MIPRTPSPEEXDECODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "miprtppacketdecoder.h"
#include "mipspeexutil.h"

/** This class decodes incoming RTP data into Speex messages.
 *  This class takes MIPRTPReceiveMessages as input and generates 
 *  Speex audio messages.
 */
class MIPRTPSpeexDecoder : public MIPRTPPacketDecoder
{
public:
	MIPRTPSpeexDecoder();
	~MIPRTPSpeexDecoder();
private:
	bool validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit);
	MIPMediaMessage *createNewMessage(const RTPPacket *pRTPPack);
	MIPSpeexUtil m_speexUtil;
	int m_sampRate;
};

#endif // MIPCONFIG_SUPPORT_SPEEX

#endif // MIPRTPSPEEXDECODER_H

