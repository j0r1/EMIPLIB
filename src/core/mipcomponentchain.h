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
 * \file mipcomponentchain.h
 */

#ifndef MIPCOMPONENTCHAIN_H

#define MIPCOMPONENTCHAIN_H

#include "mipconfig.h"
#include "miperrorbase.h"
#include "mipmessage.h"
#include <jthread.h>
#include <string>
#include <list>

class MIPComponent;

/** A chain of components.
 *  This class describes a collection of links which exist between specific components. When the
 *  chain is started, messages will be passed over these links. The messages are described by
 *  classes derived from MIPMessage, components are implemented in classes derived from MIPComponent.
 */
class MIPComponentChain : private JThread, public MIPErrorBase
{
public:
	/** Create a chain with a specific name.
	 */
	MIPComponentChain(const std::string &chainName);
	virtual ~MIPComponentChain();

	/** Start the chain.
	 *  This function analyses the connections which were added previously and starts a background
	 *  thread which will distribute the messages over the links in the chain. The chain itself
	 *  sends a MIPSystemMessage with subtype MIPSYSTEMMESSAGE_WAITTIME to the component set by
	 *  MIPComponentChain::setChainStart, instructing it to wait until it is time to process the rest of the chain.
	 *  Therefore, it is the first component of the chain which performs the timing.
	 */
	bool start();

	/** Stops the chain.
	 *  This function stops the background thread in which the messages are distributed.
	 */
	bool stop();

	/** Clears the component chain.
	 *  Removes all components from the component chain.
	 */
	bool clearChain();

	/** Set the start of the chain.
	 *  Informs the chain to which component the initial message should be sent. It is this component
	 *  which will determine the timing in the chain.
	 */
	bool setChainStart(MIPComponent *pStartComponent);

	/** Add a link between two components.
	 *  With this function, a link from pPullComponent to pPushCompontent is created. This means that at
	 *  a certain point in the chain, messages will be extracted from pPullComponent using its implementation
	 *  of MIPComponent::pull and will be fed to pPushComponent using its MIPComponent::push implementation.
	 *
	 *  Note that no copies are made of these components, the actual components specified by the MIPComponent
	 *  pointers are used in the chain. For this reason, as long as the chain will be used, these MIPComponent
	 *  objects must exist.
	 *  
	 *  Messages can be filtered both on type and on subtype. By default, all messages are allowed over a
	 *  link between components, but it is also possible to only allow specific kinds of messages to be
	 *  passed over that link. Filtering can be done using the parameters allowedMessageTypes and
	 *  allowedSubmessageTypes. For example, to only allow LPC compressed audio messages to be passed over a
	 *  connection, allowedMessageTypes should be set to MIPMESSAGE_TYPE_AUDIO_ENCODED and allowedSubmessageTypes
	 *  should be set to MIPENCODEDAUDIOMESSAGE_TYPE_LPC.
	 *
	 *  If the feedback flag is set to true, a MIPFeedback message will be passed over this link. This way,
	 *  it is possible to provide feedback (about introduced delay for example) to components higher up
	 *  in the chain.
	 */
	bool addConnection(MIPComponent *pPullComponent, MIPComponent *pPushCompontent, bool feedback = false,
	                  uint32_t allowedMessageTypes = MIPMESSAGE_TYPE_ALL, 
	                  uint32_t allowedSubmessageTypes = MIPMESSAGE_TYPE_ALL);

	/** Removes a connection previously added by the addConnection function. */
	bool deleteConnection(MIPComponent *pPullComponent, MIPComponent *pPushCompontent, bool feedback = false,
	                  uint32_t allowedMessageTypes = MIPMESSAGE_TYPE_ALL, 
	                  uint32_t allowedSubmessageTypes = MIPMESSAGE_TYPE_ALL);

	/** Returns the name of the component chain.
	 *  Returns the name of the component chain, as was specified in the constructor.
	 */
	std::string getName() const									{ return m_chainName; }
	
	/** Rebuilds a running chain. */
	bool rebuild();
protected:
	/** Function called when the background thread exits.
	 *  This function is called when the background thread exits. This can happen if the 
	 *  MIPComponentChain::stop function is called, or when an error occured in the chain.
	 *  The error flag indicates if the thread exited due to an error and if so, the
	 *  errorComponent parameter contains the name of the component which caused the error.
	 *  The errorDescription parameter then containts the error string obtained from that
	 *  component's MIPComponent::getErrorString function.
	 */
	virtual void onThreadExit(bool error, const std::string &errorComponent,
	                          const std::string &errorDescription)					{ }
private:
	class MIPConnection
	{
	public:
		MIPConnection(MIPComponent *pPull, MIPComponent *pPush, bool feedback, uint32_t mask1,
		              uint32_t mask2)								{ m_mask1 = mask1; m_mask2 = mask2; m_pPull = pPull; m_pPush = pPush; m_marked = false; m_feedback = feedback; }
		MIPComponent *getPullComponent() const							{ return m_pPull; }
		MIPComponent *getPushComponent() const							{ return m_pPush; }
		bool isMarked() const									{ return m_marked; }
		void setMark(bool v)									{ m_marked = v; }
		uint32_t getMask1() const								{ return m_mask1; }
		uint32_t getMask2() const								{ return m_mask2; }
		bool giveFeedback() const								{ return m_feedback; }
		bool operator==(const MIPConnection &c)	const						{ if (m_pPull == c.m_pPull && m_pPush == c.m_pPush && m_mask1 == c.m_mask1 && m_mask2 == c.m_mask2 && m_feedback == c.m_feedback) return true; return false; }
	private:
		MIPComponent *m_pPull, *m_pPush;
		uint32_t m_mask1;
		uint32_t m_mask2;
		bool m_marked;
		bool m_feedback;
	};

	void *Thread();
	bool orderConnections(std::list<MIPConnection> &orderedConnections);
	bool buildFeedbackList(std::list<MIPComponent *> &feedbackChain);
	void copyConnectionInfo(const std::list<MIPConnection> &orderedList, const std::list<MIPComponent *> &feedbackChain);
	
	std::string m_chainName;
	std::list<MIPConnection> m_inputConnections;
	std::list<MIPConnection> m_orderedConnections;
	std::list<MIPComponent *> m_feedbackChain;
	MIPComponent *m_pInputChainStart;	
	MIPComponent *m_pInternalChainStart;

	JMutex m_loopMutex;
	JMutex m_chainMutex;
	bool m_stopLoop;
};

#endif // MIPCOMPONENTCHAIN_H

