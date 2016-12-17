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
 * \file mipoutputmessagequeuewithstate.h
 */

#ifndef MIPOUTPUTMESSAGEQUEUEWITHSTATE_H

#define MIPOUTPUTMESSAGEQUEUEWIDTHSTATE_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <unordered_map>
#include <list>

class MIPMessage;

/** Base class for state information (e.g a decoder state).
 *  Base class for state information (e.g a decoder state), used by the MIPOutputMessageQueueWithState class.
 */
class EMIPLIB_IMPORTEXPORT MIPStateInfo
{
protected:
	MIPStateInfo();
public:
	virtual ~MIPStateInfo();

	MIPTime getLastUpdateTime() const							{ return m_lastTime; }

	/** Adjusts the time at which this state was last changed to the current time.
	 *  Adjusts the time at which this state was last changed to the current time.
	 *  This will be used to check if a specific state was used recently to be able
	 *  to free some memory if it's an 'old' state. */
	void setUpdateTime()									{ m_lastTime = MIPTime::getCurrentTime(); }
private:
	MIPTime m_lastTime;
};

/** Helper class to server as base class for components which need an
 *  output queue of messages and which also need to store state information.
 *  Helper class to server as base class for components which need an
 *  output queue of messages and which also need to store state information
 *  for stream sources (e.g. for the decoder of a compression algorithm).
 *
 *  For the output queue, the class is similar to the MIPOutputMessageQueue
 *  class, and the same \c checkIteration and \c addToOutputQueue calls
 *  should be made in the 'push' implementation. If each input message can
 *  generate only one output message, you may find the MIPOutputMessageQueueWithStateSimple
 *  class more useful.
 *
 *  To handle the state information, the class provides MIPOutputMessageQueueWithState::findState
 *  and MIPOutputMessageQueueWithState::addState calls.
 */
class EMIPLIB_IMPORTEXPORT MIPOutputMessageQueueWithState : public MIPComponent
{
protected:
	/** Constructor of the class, which passes a specific component name to the MIPComponent constructor. */
	MIPOutputMessageQueueWithState(const std::string &componentName);
public:
	~MIPOutputMessageQueueWithState();
protected:
	/** Initialize this component.
	 *  Initialize the component. This function should be called during the
	 *  initialization of you own component, specifying the time interval (in seconds) after
	 *  which state information should be timed out. */
	void init(real_t expirationDelay);

	/** Clears the messages in the output queue and the stored state information.
	 *  Clears the messages in the output queue and the stored state information, should be called to free up memory
	 *  during the de-initialization of your component. */
	void clear()										{ clearMessages(); clearStates(); }

	/** Call this at the start of your 'push' method to make sure that the output
	 *  message queue is cleared when a new iteration of your component chain is
	 *  executed. */
	void checkIteration(int64_t iteration);

	/** Add the specified message to the output queue, indicating if the message
	 *  should be deleted when the output messages queue is cleared for a new
	 *  iteration. */
	void addToOutputQueue(MIPMessage *pMsg, bool deleteMessage);

	/** Look for the state information for the source with the specified ID,
	 *  returning NULL if no state for this ID exists yet. 
	 *  Look for the state information for the source with the specified ID,
	 *  returning NULL if no state for this ID exists yet. Make sure to call
	 *  the MIPStateInfo::setUpdateTime function to avoid timeout of an active
	 *  participant.
	 */
	MIPStateInfo *findState(uint64_t id);

	/** Add state information for the new source with the specified ID.
	 *  Add state information for the new source with the specified ID. \c The
	 *  function will return \c false if an error occurs (\c id already exists
	 *  or \c pState is NULL). On success, \c true is returned and the state
	 *  can be retrieved again using the MIPOutputMessageQueueWithState::findState
	 *  function. */
	bool addState(uint64_t id, MIPStateInfo *pState);
private:
	void clearMessages();
	void clearStates();
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	void expire();

	std::list<std::pair<MIPMessage *, bool> > m_messages;
	std::list<std::pair<MIPMessage *, bool> >::const_iterator m_msgIt;
	int64_t m_prevIteration;

	double m_expirationDelay;
	MIPTime m_lastExpireTime;
	std::unordered_map<uint64_t, MIPStateInfo *> m_states;
};

#endif // MIPOUTPUTMESSAGEQUEUEWITHSTATE_H

