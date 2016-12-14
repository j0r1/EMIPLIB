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
 * \file mipinterchaintimer.h
 */

#ifndef MIPINTERCHAINTIMER_H

#define MIPINTERCHAINTIMER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "mipsystemmessage.h"
#include "mipsignalwaiter.h"
#include "miptime.h"
#include <jthread/jthread.h>

/** Component which generates a MIPSYSTEMMESSAGE_TYPE_ISTIME system message when data has
 *  been supplied to it from another chain. 
 *  This component generates a MIPSYSTEMMESSAGE_TYPE_ISTIME system message when a
 *  component from another thread has delivered data to its 'trigger component'. The
 *  trigger component accepts any incoming message; the main component only accepts
 *  a MIPSYSTEMMESSAGE_TYPE_WAITTIME message.
 */
class EMIPLIB_IMPORTEXPORT MIPInterChainTimer : public MIPComponent
{
public:
	MIPInterChainTimer();
	~MIPInterChainTimer();
	
	/** Initializes the component.
	 *  To initialize the component, this function must be called first. When
	 *  the initialization was succesfull, an internal 'trigger component'
	 *  is created, which can accept messages from another thread. When this
	 *  happens, a signal is sent to the main component, causing it to
	 *  generate a MIPSYSTEMMESSAGE_TYPE_ISTIME message.
	 *  \param count Instead of sending a signal to the main component on each
	 *               incoming message, it is also possible to skip a number
	 *               of messages each time. If this parameter is 1 or less,
	 *               each message will cause a signal to be sent. If the value
	 *               is 2 for example, every other message will cause a signal
	 *               to be sent.
	 *  \param safetyTimeout Because this component will be waiting for signals
	 *                       from another chain, problems would arise if that
	 *                       chain crashed for example. To avoid this, a background
	 *                       thread is started which checks if a signal has been
	 *                       sent in the time interval specified by \c safetyTimeout.
	 *                       If not, the thread itself will send a signal to force
	 *                       progress in this component's chain.
	 */
	bool init(int count = 1, MIPTime safetyTimeout = MIPTime(0.5));
	
	/** De-initializes the component. */
	bool destroy();

	/** Returns a pointer to the internal 'trigger component'.
	 *  Returns a pointer to the internal 'trigger component'. Note that the init
	 *  function has to be called to create such a component.
	 */
	MIPComponent *getTriggerComponent();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	class TriggerComponent : public MIPComponent, public jthread::JThread
	{
	public:
		TriggerComponent(MIPSignalWaiter &sigWait, int count) : MIPComponent("MIPInterChainTimer::TriggerComponent"), m_sigWait(sigWait)
		{
			m_targetCount = count;
			m_counter = 0;
		}

		~TriggerComponent()
		{
			stopSafetyThread();
		}
		
		bool startSafetyThread(MIPTime timeout);
		void stopSafetyThread();
		
		bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
		bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	private:
		void *Thread();

		MIPSignalWaiter &m_sigWait;
		int m_targetCount;
		int m_counter;
		MIPTime m_prevTime;
		MIPTime m_timeout;
		jthread::JMutex m_stopMutex;
		bool m_stopThread;
	};

	MIPSignalWaiter m_sigWait;
	TriggerComponent *m_pTriggerComp;
	bool m_init;
	MIPSystemMessage m_timeMsg;
	bool m_gotMsg;
	int64_t m_prevIteration;
};

#endif // MIPINTERCHAINTIMER_H

