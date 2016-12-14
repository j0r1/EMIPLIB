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

/**
 * \file miprtpvideoencoder.h
 */

#ifndef MIPRTPVIDEOENCODER_H

#define MIPRTPVIDEOENCODER_H

#include "mipconfig.h"
#include "miprtpencoder.h"
#include <list>

class MIPRTPSendMessage;

/** Creates RTP packets for incoming video packets.
 *  This component accepts incoming video packets and generates MIPRTPSendMessage
 *  objects which can then be transferred to a MIPRTPComponent instance.
 */
class MIPRTPVideoEncoder : public MIPRTPEncoder
{
public:
	MIPRTPVideoEncoder();
	~MIPRTPVideoEncoder();

	/** Initializes the encoder. 
	 *  Initializes the encoder.
	 *  \param frameRate Frame rate of incoming video frames. 
	 */
	bool init(real_t frameRate);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull (const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();

	bool m_init;
	real_t m_frameRate;
	std::list<MIPRTPSendMessage *> m_messages;
	std::list<MIPRTPSendMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPRTPVIDEOENCODER_H

