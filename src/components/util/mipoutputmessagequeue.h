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
 * \file mipoutputmessagequeue.h
 */

#ifndef MIPOUTPUTMESSAGEQUEUE_H

#define MIPOUTPUTMESSAGEQUEUE_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>

class MIPMessage;

/** Helper class to server as base class for components which need an
 *  output queue of messages.
 *  Helper class to server as base class for components which need an
 *  output queue of messages. In this version, you should write your
 *  own 'push' method, which first calls MIPOutputMessageQueue::checkIteration and stores
 *  outgoing messages using the MIPOutputMessageQueue::addToOutputQueue method. This
 *  way, a single incoming MIPMessage instance can cause multiple outgoing
 *  messages. If you're certain that for each incoming message there will
 *  only be a single outgoing message, MIPOutputMessageQueueSimple may be
 *  more appropriate.
 */
class EMIPLIB_IMPORTEXPORT MIPOutputMessageQueue : public MIPComponent
{
protected:
	/** Constructor of the class, which passes a specific component name to the MIPComponent constructor. */
 	MIPOutputMessageQueue(const std::string &componentName);
public:
	~MIPOutputMessageQueue();
protected:
	/** Initialize this component.
	 *  Initialize the component. This function should be called during the
	 *  initialization of you own component. */
	void init();

	/** Clears the messages in the output queue.
	 *  Clears the messages in the output queue, should be called to free up memory
	 *  during the de-initialization of your component. */
	void clearMessages();

	/** Call this at the start of your 'push' method to make sure that the output
	 *  message queue is cleared when a new iteration of your component chain is
	 *  executed. */
	void checkIteration(int64_t iteration);

	/** Add the specified message to the output queue, indicating if the message
	 *  should be deleted when the output messages queue is cleared for a new
	 *  iteration. */
	void addToOutputQueue(MIPMessage *pMsg, bool deleteMessage);
private:
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);

	std::list<std::pair<MIPMessage *, bool> > m_messages;
	std::list<std::pair<MIPMessage *, bool> >::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPOUTPUTMESSAGEQUEUE_H

