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
 * \file mipmediabuffer.h
 */

#ifndef MIPMEDIABUFFER_H

#define MIPMEDIABUFFER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>

class MIPMediaMessage;

/** Holds incoming messages until they're needed by a mixer further on in the chain.
 *  This component accepts all kinds of media messages and stores incoming messages.
 *  To be able to function correctly, the component needs to receive feedback information
 *  from a mixer further on in the chain. Each iteration, the component outputs the
 *  messages required by the mixer in that interval. Furthermore, the messages are
 *  output in an ordered manner, beginning with the message with the smallest time
 *  setting.
 *  
 *  To see why this component is useful, consider the following part of a component
 *  chain:
 *  \image html nobufferchain.png
 *  \image latex nobufferchain.eps
 *
 *  In the example above, it is assumed that feedback information flows from the mixer
 *  to the RTP decoder. The way the example works is as follows: an RTP component
 *  sends received RTP packets to the RTP decoder. The decoder will calculate the
 *  timing information to be used when the data will be inserted in the mixer's
 *  output stream and will create an appropriate message. A certain amount of buffering
 *  will be introduced to compensate for jitter. Let's assume that the
 *  data which was received is Speex encoded audio data, which is why we'll send it
 *  to a Speex decoder component. This component will transform the Speex data into
 *  raw audio data and will pass this to the mixer which will insert the raw audio
 *  data in the right position.
 *
 *  The problem with this setup is that packets are passed to the Speex decoder in the
 *  same order as they're received by the RTP component. When network conditions are
 *  bad however, packets can arrive out of order frequently. Since the Speex decoder
 *  uses an internal state based upon the processed packets, feeding packets to the
 *  decoder in the wrong order will case this state to become corrupted and very
 *  bad quality audio will be produced. So, although buffering is introduced to compensate
 *  for jitter, the quality of the sound will still be bad since the packets arrive
 *  at the Speex decoder in the wrong order.
 *
 *  The solution to the problem is to place a MIPMediaBuffer instance in the chain,
 *  in the following way:
 *  \image html bufferchain.png
 *  \image latex bufferchain.eps
 *
 *  As explained above, the MIPMediaBuffer instance will store incoming messages. At
 *  each iteration, it will inspect which messages will be needed by the mixer during
 *  this iteration. These messages will be ordered and sent to the output of the
 *  component. This way, the amount of buffering introduced to compensate for jitter
 *  is used to try to send the messages to the Speex decoder in the right order. Note
 *  that the component itself does not introduce extra delay.
 */
class MIPMediaBuffer : public MIPComponent
{
public:
	MIPMediaBuffer();
	~MIPMediaBuffer();

	/** Initializes the component.
	 *  Using this function, the component is initialized. The \c interval parameter
	 *  should be the same as the one used in the mixer further on in the chain.
	 */
	bool init(MIPTime interval);

	/** De-initializes the component. */
	bool destroy();
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	bool processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback);
private:
	void clearMessages();
	void clearBuffers();
	void buildOutputMessages();
	void insertInOutput(MIPMediaMessage *pMsg);

	bool m_init;
	std::list<MIPMediaMessage *> m_messages;
	std::list<MIPMediaMessage *> m_buffers;
	std::list<MIPMediaMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
	MIPTime m_interval, m_playbackTime;
	bool m_gotPlaybackTime;
};

#endif // MIPMEDIABUFFER_H

