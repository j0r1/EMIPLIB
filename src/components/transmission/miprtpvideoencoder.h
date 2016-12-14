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
 *  objects which can then be transferred to a MIPRTPComponent instance. This
 *  encoder uses an internal format to store data in RTP packets, this will not
 *  be compatible with other applications.
 */
class MIPRTPVideoEncoder : public MIPRTPEncoder
{
public:
	/** Specifies the kind of message that will be accepted as input. */
	enum EncodingType 
	{ 
		YUV420P, 	/**< Raw YUV420P messages. */
		H263 		/**< H.263 encoded messages. */
	};

	MIPRTPVideoEncoder();
	~MIPRTPVideoEncoder();

	/** Initializes the encoder. 
	 *  Initializes the encoder.
	 *  \param frameRate Frame rate of incoming video frames. 
	 *  \param maxPayloadSize Maximum size the payload of an RTP packet can be. If 
	 *                        there's more data, it will be split over multiple
	 *                        RTP packets.
	 *  \param encType Specifies the kind of messages that will be accepted as input
	 *                 in the 'push' method of this component.
	 */
	bool init(real_t frameRate, size_t maxPayloadSize, EncodingType encType);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();

	bool m_init;
	real_t m_frameRate;
	std::list<MIPRTPSendMessage *> m_messages;
	std::list<MIPRTPSendMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;

	uint8_t m_encodingType;
	uint32_t m_encMsgType, m_encSubMsgType;
	size_t m_maxPayloadSize;
};

#endif // MIPRTPVIDEOENCODER_H

