/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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
 * \file mipoutputmessagequeuewithstatesimple.h
 */

#ifndef MIPOUTPUTMESSAGEQUEUEWITHSTATESIMPLE_H

#define MIPOUTPUTMESSAGEQUEUEWITHSTATESIMPLE_H

#include "mipconfig.h"
#include "mipoutputmessagequeuewithstate.h"

class MIPMessage;

/** Helper class to server as base class for components which need an
 *  output queue of messages and which also need to store state information.
 *  Helper class to server as base class for components which need an
 *  output queue of messages and which also need to store state information
 *  for stream sources (e.g. for the decoder of a compression algorithm).
 *
 *  For the output queue, the class is similar to the MIPOutputMessageQueueSimple
 *  class, so the MIPOutputMessageQueueWithStateSimple::subPush method should
 *  be implemented to create an output message for each input message.
 *  If you need multiple output messages for an incoming message, you'll need
 *  to use the MIPOutputMessageQueueWithState class.
 *
 *  To handle the state information, the class uses the MIPOutputMessageQueueWithState::findState
 *  and MIPOutputMessageQueueWithState::addState calls. Make sure to use the
 *  MIPOutputMessageQueueWithState::init and MIPOutputMessageQueueWithState::clear functions
 *  as well.
 */
class EMIPLIB_IMPORTEXPORT MIPOutputMessageQueueWithStateSimple : public MIPOutputMessageQueueWithState
{
protected:
	/** Constructor of the class, which passes a specific component name to the MIPComponent constructor. */
	MIPOutputMessageQueueWithStateSimple(const std::string &componentName);
public:
	~MIPOutputMessageQueueWithStateSimple();
protected:
	/** This function must be implemented in your class to generate an output message
	 *  for a specific input message.
	 *  This function must be implemented in your class to generate an output message
	 *  for a specific input message. The message will automatically be queued in the
	 *  output queue. 
	 *
	 *  The three first parameters are the same as the ones from the MIPComponent::push
	 *  function. The \c deleteMessage parameter defaults to \c true and indicates if the
	 *  returned message should be deleted when clearing the output queue. The \c error
	 *  parameter defaults to \c false and should be set to \c true if an error occured
	 *  that should be reported by the MIPComponent::push function (this will stop the
	 *  component chain).
	 *
	 *  If you return NULL, the message \c pMsg will simply be ignored and no error will
	 *  be generated.
	 */
	virtual MIPMessage *subPush(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg, bool &deleteMessage, bool &error) = 0;
private:
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg) final;
};

#endif // MIPOUTPUTMESSAGEQUEUEWITHSTATESIMPLE_H

