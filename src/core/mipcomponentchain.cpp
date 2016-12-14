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

#include "mipconfig.h"
#include "mipcomponentchain.h"
#include "mipsystemmessage.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipfeedback.h"
#include <iostream>

#include "mipdebug.h"

#define MIPCOMPONENTCHAIN_ERRSTR_CANTSTARTTHREAD 	"Can't start background thread"
#define MIPCOMPONENTCHAIN_ERRSTR_THREADRUNNING	 	"Background thread is running"
#define MIPCOMPONENTCHAIN_ERRSTR_NOSTARTCOMPONENT 	"No start component was defined"
#define MIPCOMPONENTCHAIN_ERRSTR_THREADNOTRUNNING 	"Background thread is not running"
#define MIPCOMPONENTCHAIN_ERRSTR_COMPONENTNULL		"A NULL component pointer was passed"
#define MIPCOMPONENTCHAIN_ERRSTR_UNUSEDCONNECTION	"Detected an unused connection"
#define MIPCOMPONENTCHAIN_ERRSTR_CANTMERGEFEEDBACK	"Can't merge multiple feedback chains"

MIPComponentChain::MIPComponentChain(const std::string &chainName)
{
	int status;
	
	if ((status = m_loopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize component chain mutex (JMutex error code " << status << ")" << std::endl; 
		exit(-1);
	}
	m_chainName = chainName;
	m_pChainStart = 0;
}

MIPComponentChain::~MIPComponentChain()
{
	stop();
}

bool MIPComponentChain::start()
{
	if (JThread::IsRunning())
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_THREADRUNNING);
		return false;
	}
	
	if (m_pChainStart == 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_NOSTARTCOMPONENT);
		return false;
	}

	if (!orderConnections())
		return false;
	if (!buildFeedbackList())
		return false;

	m_stopLoop = false;
	if (JThread::Start() < 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_CANTSTARTTHREAD);
		return false;
	}
	return true;
}

bool MIPComponentChain::stop()
{
	if (!JThread::IsRunning())
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_THREADNOTRUNNING);
		return false;
	}

	m_loopMutex.Lock();
	m_stopLoop = true;
	m_loopMutex.Unlock();
	
	MIPTime curTime = MIPTime::getCurrentTime();
	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue() - curTime.getValue()) < 5.0) // wait maximum five seconds
	{
		MIPTime::wait(MIPTime(0.010));
	}

	if (JThread::IsRunning())
		JThread::Kill();
	
	return true;
}

bool MIPComponentChain::clearChain()
{
	if (JThread::IsRunning())
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_THREADRUNNING);
		return false;
	}
	m_connections.clear();
	m_pChainStart = 0;
	return true;
}

bool MIPComponentChain::setChainStart(MIPComponent *startComponent)
{
	if (JThread::IsRunning())
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_THREADRUNNING);
		return false;
	}
	m_pChainStart = startComponent;
	return true;
}

bool MIPComponentChain::addConnection(MIPComponent *pPullComponent, MIPComponent *pPushComponent, bool feedback,
		                     uint32_t allowedMessageTypes, uint32_t allowedSubmessageTypes)
{
	if (JThread::IsRunning())
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_THREADRUNNING);
		return false;
	}
	if (pPullComponent == 0 || pPushComponent == 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_COMPONENTNULL);
		return false;
	}
	
	uint64_t mask = (((uint64_t)allowedMessageTypes) << 32) | ((uint64_t)allowedSubmessageTypes);

	m_connections.push_back(MIPConnection(pPullComponent, pPushComponent, feedback, mask));
	return true;
}

void *MIPComponentChain::Thread()
{
	bool done = false;
	bool error = false;
	int64_t iteration = 1;
	std::string errorComponent, errorString;
	
	m_loopMutex.Lock();
	done = m_stopLoop;
	m_loopMutex.Unlock();
	
	JThread::ThreadStarted();
	
	MIPSystemMessage startMsg(MIPSYSTEMMESSAGE_TYPE_WAITTIME);
	
	while (!done && !error)
	{
		m_pChainStart->lock();
//		std::cout << m_chainName << " START " << std::endl;
//		std::cout << m_chainName << " push start: " << m_pChainStart->getComponentName() << std::endl;
		if (!m_pChainStart->push(*this, iteration, &startMsg))
		{
			error = true;
			errorComponent = m_pChainStart->getComponentName();
			errorString = m_pChainStart->getErrorString();
			m_pChainStart->unlock();
			break;
		}
//		std::cout << m_chainName << " push stop:  " << m_pChainStart->getComponentName() << std::endl;
		m_pChainStart->unlock();

#ifdef MIPDEBUG
	//	MIPTime curTime = MIPTime::getCurrentTime();
#endif // MIPDEBUG
		
		std::list<MIPConnection>::const_iterator it;

		for (it = m_connections.begin() ; !error && it != m_connections.end() ; it++)
		{
			MIPComponent *pPullComp = (*it).getPullComponent();
			MIPComponent *pPushComp = (*it).getPushComponent();
			uint64_t mask = (*it).getMask();

			pPullComp->lock();
			if (pPushComp != pPullComp)
				pPushComp->lock();

			MIPMessage *msg = 0;

			do
			{
//				std::cout << m_chainName << " pull start: " << pPullComp->getComponentName() << std::endl;
				if (!pPullComp->pull(*this, iteration, &msg))
				{
					error = true;
					errorComponent = pPullComp->getComponentName();
					errorString = pPullComp->getErrorString();
				}
				else
				{
//					std::cout << m_chainName << " pull stop:  " << pPullComp->getComponentName() << std::endl;
					if (msg) // Ok, pass the message
					{
						uint64_t msgType = msg->getTotalType();

						if (msgType&mask)
						{
//							std::cout << m_chainName << " push start: " << pPushComp->getComponentName() << std::endl;
							if(!pPushComp->push(*this, iteration, msg))
							{
								error = true;
								errorComponent = pPushComp->getComponentName();
								errorString = pPushComp->getErrorString();
							}
//							std::cout << m_chainName << " push stop:  " << pPushComp->getComponentName() << std::endl;
						}
					}
				}
			} while (!error && msg);
			
			pPullComp->unlock();
			if (pPushComp != pPullComp)
				pPushComp->unlock();
		}

		if (error)
			break;

		std::list<MIPComponent *>::const_iterator fbIt;
		MIPFeedback feedback;
		int64_t chainID = 0;
		
		//std::cerr << "Processing feedback:" << std::endl;
		
		for (fbIt = m_feedbackChain.begin() ; !error && fbIt != m_feedbackChain.end() ; fbIt++)
		{
			if ((*fbIt) == 0)
			{
				feedback = MIPFeedback(); // reinitialize feedback
				chainID++;
				//std::cerr << "\tNEW CHAIN" << std::endl;
			}
			else
			{
				MIPComponent *pFbComp = *fbIt;
				pFbComp->lock();
				//std::cerr << "\t" << pFbComp->getComponentName() << std::endl;
				if (!pFbComp->processFeedback(*this, chainID, &feedback))
				{
					error = true;
					errorComponent = pFbComp->getComponentName();
					errorString = pFbComp->getErrorString();
				}
				pFbComp->unlock();
			}
		}

		if (error)
			break;

		//std::cerr << std::endl;
		
		m_loopMutex.Lock();
		done = m_stopLoop;
		m_loopMutex.Unlock();
		iteration++;

#ifdef MIPDEBUG
//		MIPTime diff = MIPTime::getCurrentTime();
//		diff -= curTime;
//		std::cout << "Loop time: " << diff.getString() << std::endl;
#endif // MIPDEBUG
	}
	
	onThreadExit(error, errorComponent, errorString);
	
	return 0;
}

bool MIPComponentChain::orderConnections()
{
	std::list<MIPConnection> orderedList;
	std::list<MIPConnection>::iterator it;
	std::list<MIPComponent *> componentLayer;

	for (it = m_connections.begin() ; it != m_connections.end() ; it++)
		(*it).setMark(false);
	
	componentLayer.push_back(m_pChainStart);
	while (!componentLayer.empty())
	{
		std::list<MIPComponent *> newLayer;
		std::list<MIPComponent *>::const_iterator compit;
		
		for (compit = componentLayer.begin() ; compit != componentLayer.end() ; compit++)
		{
			for (it = m_connections.begin() ; it != m_connections.end() ; it++)
			{
				if (!(*it).isMarked()) // check that we haven't processed this connection yet
				{
					if ((*it).getPullComponent() == (*compit)) // check that this connection starts from the component under consideration
					{
						// mark the connection as processed
						(*it).setMark(true);
						// copy the connection in the ordered list
						orderedList.push_back(*it);

						// get the other end of the connection and add that
						// component to the new layer
						// we'll make sure that the component isn't already
						// in the list

						bool found = false;
						MIPComponent *component = (*it).getPushComponent();
						std::list<MIPComponent *>::const_iterator compit2;

						for (compit2 = newLayer.begin() ; !found && compit2 != newLayer.end() ; compit2++)
						{
							if ((*compit2) == component)
								found = true;
						}

						if (!found)
							newLayer.push_back(component);
					}
				}
			}
		}
		
		componentLayer = newLayer;
	}
	
	for (it = m_connections.begin() ; it != m_connections.end() ; it++)
	{
		if (!(*it).isMarked())
		{
			setErrorString(MIPCOMPONENTCHAIN_ERRSTR_UNUSEDCONNECTION);
			return false;		
		}
	}

	m_connections = orderedList;
	
	return true;
}

bool MIPComponentChain::buildFeedbackList()
{
	std::list<MIPConnection>::iterator it;
	std::list<MIPComponent *> subChain;
	bool done = false;
	
	for (it = m_connections.begin() ; it != m_connections.end() ; it++)
	{
		if (!(*it).giveFeedback())
			(*it).setMark(true);
		else
			(*it).setMark(false);
	}
	
	m_feedbackChain.clear();

	while (!done)
	{
		bool found = false;

		it = m_connections.begin();
		while (!found && it != m_connections.end())
		{
			if (!(*it).isMarked() && (*it).giveFeedback())
				found = true;
			else
				it++;
		}

		if (found) // ok, found a starting point, build the subchain
		{
			(*it).setMark(true);
			
			// The connections are ordered! We'll make use of this by only
			// considering connections further down the road
			
			subChain.clear();

			// connection is from pull to push
			subChain.push_back((*it).getPullComponent());
			subChain.push_back((*it).getPushComponent());

			(*it).setMark(true);

			std::list<MIPConnection>::iterator startIt = it;
			bool done2 = false;
			
			while (!done2)
			{
				std::list<MIPConnection>::iterator nextStartIt;
				bool foundNextStartIt = false;

				it = startIt;
				it++;
				while (it != m_connections.end())
				{
					if (!(*it).isMarked() && (*it).giveFeedback())
					{
						if ((*it).getPullComponent() == (*startIt).getPushComponent())
						{
							if (foundNextStartIt)
							{
								setErrorString(MIPCOMPONENTCHAIN_ERRSTR_CANTMERGEFEEDBACK);
								return false;
							}
							foundNextStartIt = true;
							nextStartIt = it;
							subChain.push_back((*it).getPushComponent());
							(*it).setMark(true);
						}
					}
					it++;
				}

				if (!foundNextStartIt)
					done2 = true;
				else
					startIt = nextStartIt;
			}

			// add the subchain to the feedbacklist in reverse
		
			if (!m_feedbackChain.empty())
				m_feedbackChain.push_front(0); // mark new subchain

			std::list<MIPComponent *>::const_iterator it2;

			for (it2 = subChain.begin() ; it2 != subChain.end() ; it2++)
				m_feedbackChain.push_front(*it2);
		}
		else
			done = true;
	}
	
	return true;
}

