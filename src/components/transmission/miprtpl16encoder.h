/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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
 * \file miprtpl16encoder.h
 */

#ifndef MIPRTPL16ENCODER_H

#define MIPRTPL16ENCODER_H

#include "mipconfig.h"
#include "miprtpencoder.h"
#include <list>

class MIPRTPSendMessage;

/** Creates RTP packets for 16 bit signed big endian encoded audio packets (44100Hz).
 *  This component accepts incoming 44100Hz mono or stereo audio packets (16 bit signed
 *  big endian) and generates MIPRTPSendMessage objects which can then be transferred 
 *  to a MIPRTPComponent instance.
 */
class MIPRTPL16Encoder : public MIPRTPEncoder
{
public:
	MIPRTPL16Encoder();
	~MIPRTPL16Encoder();

	/** Initializes the encoder for mono or stereo audio messages. */
	bool init(bool stereo);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();

	bool m_init;
	int m_channels;
	std::list<MIPRTPSendMessage *> m_messages;
	std::list<MIPRTPSendMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPRTPL16ENCODER_H

