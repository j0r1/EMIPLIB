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

#include "mipconfig.h"
#include "mipinterchaintimer.h"

#include "mipdebug.h"

#define MIPINTERCHAINTIMER_ERRSTR_ALREADYINITIALIZED		"Already initialized"
#define MIPINTERCHAINTIMER_ERRSTR_CANTINITSIGWAIT		"Couldn't initialize the signal waiter"
#define MIPINTERCHAINTIMER_ERRSTR_NOTINIT			"Not initialized"
#define MIPINTERCHAINTIMER_ERRSTR_PULLNOTSUPPORTED		"Pull is not supported for this component"
#define MIPINTERCHAINTIMER_ERRSTR_BADMESSAGE			"Only a MIPSYSTEMMESSAGE_TYPE_WAITTIME message is allowed here"
#define MIPINTERCHAINTIMER_ERRSTR_CANTINITMUTEX			"Unable to initialize the stop mutex"
#define MIPINTERCHAINTIMER_ERRSTR_CANTSTARTTHREAD		"Unable to start the safety thread"

MIPInterChainTimer::MIPInterChainTimer() : MIPComponent("MIPInterChainTimer"), m_timeMsg(MIPSYSTEMMESSAGE_TYPE_ISTIME)
{
	m_init = false;
}

MIPInterChainTimer::~MIPInterChainTimer()
{
	destroy();
}

bool MIPInterChainTimer::init(int count, MIPTime safetyTimeout)
{
	if (m_init)
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_ALREADYINITIALIZED);
		return false;
	}
	
	if (!m_sigWait.init())
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_CANTINITSIGWAIT);
		return false;
	}
	
	m_pTriggerComp = new TriggerComponent(m_sigWait, count);

	if (!m_pTriggerComp->startSafetyThread(safetyTimeout))
	{
		setErrorString(m_pTriggerComp->getErrorString());
		delete m_pTriggerComp;
		return false;
	}
	
	m_gotMsg = false;
	m_prevIteration = -1;
	m_init = true;

	return true;
}

bool MIPInterChainTimer::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_NOTINIT);
		return false;
	}

	delete m_pTriggerComp;

	m_sigWait.destroy();
	
	m_init = false;
	return true;
}

MIPComponent *MIPInterChainTimer::getTriggerComponent()
{
	if (!m_init)
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_NOTINIT);
		return 0;
	}
	return m_pTriggerComp;
}

bool MIPInterChainTimer::TriggerComponent::startSafetyThread(MIPTime timeout)
{
	if (!m_stopMutex.IsInitialized())
	{
		if (m_stopMutex.Init() < 0)
		{
			setErrorString(MIPINTERCHAINTIMER_ERRSTR_CANTINITMUTEX);
			return false;
		}
	}

	m_stopThread = false;
	m_timeout = timeout;
	m_prevTime = MIPTime::getCurrentTime();

	if (JThread::Start() < 0)
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_CANTSTARTTHREAD);
		return false;
	}
	
	return true;
}

void MIPInterChainTimer::TriggerComponent::stopSafetyThread()
{
	m_stopMutex.Lock();
	m_stopThread = true;
	m_stopMutex.Unlock();

	MIPTime startTime = MIPTime::getCurrentTime();
	
	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue() - startTime.getValue()) < m_timeout.getValue()*2.0) 
	{
		MIPTime::wait(MIPTime(0.5));
	}

	if (JThread::IsRunning())
		JThread::Kill();
}

void *MIPInterChainTimer::TriggerComponent::Thread()
{
	JThread::ThreadStarted();

	m_stopMutex.Lock();
	bool stopThread = m_stopThread;
	m_stopMutex.Unlock();

	while (!stopThread)
	{
		MIPTime::wait(m_timeout.getValue()/4.0);

		MIPComponent::lock();
		MIPTime prevTime = m_prevTime;
		MIPComponent::unlock();

		MIPTime diff = MIPTime::getCurrentTime();
		diff -= prevTime;

		if (diff > m_timeout)
			m_sigWait.signal();
		
		m_stopMutex.Lock();
		stopThread = m_stopThread;
		m_stopMutex.Unlock();
	}
	
	return 0;
}

bool MIPInterChainTimer::TriggerComponent::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	m_counter++;
	if (m_counter >= m_targetCount)
	{
		m_counter = 0;
		m_sigWait.signal();
	}

	m_prevTime = MIPTime::getCurrentTime(); // the component is already locked, no need to lock it again
	return true;
}

bool MIPInterChainTimer::TriggerComponent::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPINTERCHAINTIMER_ERRSTR_PULLNOTSUPPORTED);
	return false;
}

bool MIPInterChainTimer::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_NOTINIT);
		return false;
	}

	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME)
	{
		m_sigWait.waitForSignal();
	
		m_prevIteration = iteration;
		m_gotMsg = false;
	}
	else
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPInterChainTimer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPINTERCHAINTIMER_ERRSTR_NOTINIT);
		return false;
	}

	if (m_prevIteration != iteration) // no event received
	{
		*pMsg = 0;
		return true;
	}
	
	if (!m_gotMsg)
	{
		*pMsg = &m_timeMsg;
		m_gotMsg = true;
	}
	else
	{
		*pMsg = 0;
		m_gotMsg = false;
	}
	return true;
}

