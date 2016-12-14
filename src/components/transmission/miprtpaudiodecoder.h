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
 * \file miprtpaudiodecoder.h
 */

#ifndef MIPRTPAUDIODECODER_H

#define MIPRTPAUDIODECODER_H

#include "mipconfig.h"
#include "miprtpdecoder.h"

/** This component decodes incoming RTP data into audio messages.
 *  This component takes MIPRTPReceiveMessages as input and generates 
 *  audio messages. Most of the functionality is provided by the base
 *  class MIPRTPDecoder.
 */
class MIPRTPAudioDecoder : public MIPRTPDecoder
{
public:
	MIPRTPAudioDecoder();
	~MIPRTPAudioDecoder();
private:
	bool validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit);
	MIPMediaMessage *createNewMessage(const RTPPacket *pRTPPack);

	int m_sampRate;
	int m_bytesPerSample;
	bool m_encoded;
	bool m_stereo;
	int m_encodingType;
};

#endif // MIPRTPAUDIODECODER_H

