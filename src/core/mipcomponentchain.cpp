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

#include "mipconfig.h"
#include "mipcomponentchain.h"
#include "mipsystemmessage.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipfeedback.h"
#include <cstdlib>
#include <iostream>

#include "mipdebug.h"

//#define MIPDEBUG2
//#define MIPDEBUG3
//#define MIPDEBUG4

#define MIPCOMPONENTCHAIN_ERRSTR_CANTSTARTTHREAD 	"Can't start background thread"
#define MIPCOMPONENTCHAIN_ERRSTR_THREADRUNNING	 	"Background thread is running"
#define MIPCOMPONENTCHAIN_ERRSTR_NOSTARTCOMPONENT 	"No start component was defined"
#define MIPCOMPONENTCHAIN_ERRSTR_THREADNOTRUNNING 	"Background thread is not running"
#define MIPCOMPONENTCHAIN_ERRSTR_COMPONENTNULL		"A NULL component pointer was passed"
#define MIPCOMPONENTCHAIN_ERRSTR_UNUSEDCONNECTION	"Detected an unused connection"
#define MIPCOMPONENTCHAIN_ERRSTR_CANTMERGEFEEDBACK	"Can't merge multiple feedback chains"
#define MIPCOMPONENTCHAIN_ERRSTR_CONNECTIONNOTFOUND	"Connection not found"

MIPComponentChain::MIPComponentChain(const std::string &chainName)
{
	// We're forwarding this getCurrentTime call to something that needs initialization,
	// so we'll just call it as soon as possible here, so that the initialization is
	// already performed by the time we're using threads
	m_dummy = (uint32_t)MIPTime::getCurrentTime().getSeconds();

	int status;
	
	if ((status = m_loopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize loop mutex (JMutex error code " << status << ")" << std::endl; 
		exit(-1);
	}
	if ((status = m_chainMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize component chain mutex (JMutex error code " << status << ")" << std::endl; 
		exit(-1);
	}
	m_chainName = chainName;
	m_pInputChainStart = 0;
	m_pInternalChainStart = 0;
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
	
	if (m_pInputChainStart == 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_NOSTARTCOMPONENT);
		return false;
	}

	std::list<MIPConnection> orderedList;
	std::list<MIPComponent *> feedbackChain;
	
	if (!orderConnections(orderedList))
		return false;
	if (!buildFeedbackList(orderedList, feedbackChain))
		return false;

	copyConnectionInfo(orderedList, feedbackChain);

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

bool MIPComponentChain::rebuild()
{
	if (!JThread::IsRunning())
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_THREADNOTRUNNING);
		return false;
	}

	if (m_pInputChainStart == 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_NOSTARTCOMPONENT);
		return false;
	}

	std::list<MIPConnection> orderedList;
	std::list<MIPComponent *> feedbackChain;
	
	if (!orderConnections(orderedList))
		return false;
	if (!buildFeedbackList(orderedList, feedbackChain))
		return false;

	copyConnectionInfo(orderedList, feedbackChain);
	
	return true;
}

bool MIPComponentChain::clearChain()
{
	m_inputConnections.clear();
	m_pInputChainStart = 0;
	return true;
}

bool MIPComponentChain::setChainStart(MIPComponent *startComponent)
{
	if (startComponent == 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_COMPONENTNULL);
		return false;
	}
	m_pInputChainStart = startComponent;
	return true;
}

bool MIPComponentChain::addConnection(MIPComponent *pPullComponent, MIPComponent *pPushComponent, bool feedback,
		                     uint32_t allowedMessageTypes, uint32_t allowedSubmessageTypes)
{
	if (pPullComponent == 0 || pPushComponent == 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_COMPONENTNULL);
		return false;
	}
	
	m_inputConnections.push_back(MIPConnection(pPullComponent, pPushComponent, feedback, allowedMessageTypes, allowedSubmessageTypes));
	return true;
}

bool MIPComponentChain::deleteConnection(MIPComponent *pPullComponent, MIPComponent *pPushComponent, bool feedback,
		                     uint32_t allowedMessageTypes, uint32_t allowedSubmessageTypes)
{
	if (pPullComponent == 0 || pPushComponent == 0)
	{
		setErrorString(MIPCOMPONENTCHAIN_ERRSTR_COMPONENTNULL);
		return false;
	}
	
	std::list<MIPConnection>::iterator it;
	MIPConnection conn(pPullComponent, pPushComponent, feedback, allowedMessageTypes, allowedSubmessageTypes);

	it = m_inputConnections.begin();
	while (it != m_inputConnections.end())
	{
		if ((*it) == conn)
		{
			m_inputConnections.erase(it);
			return true;
		}
		else
			it++;
	}

	setErrorString(MIPCOMPONENTCHAIN_ERRSTR_CONNECTIONNOTFOUND);
	return false;
}

void *MIPComponentChain::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPComponentChain::Thread started" << std::endl;
#endif // MIPDEBUG

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
		MIPTime::wait(MIPTime(0,0));
		m_chainMutex.Lock();
		m_pInternalChainStart->lock();
#ifdef MIPDEBUG2
		std::cout << std::endl << m_chainName << " START " << iteration << std::endl;
		std::cout << m_chainName << " push start: " << m_pInternalChainStart->getComponentName() << std::endl;
#endif // MIPDEBUG2
#ifdef MIPDEBUG3
		std::cout << std::endl << "I " << iteration << " start in chain \"" << m_chainName << std::endl;
		std::cout << "    pushing WaitTime to: " << m_pInternalChainStart->getComponentName() << std::endl;
#endif // MIPDEBUG3

		if (!m_pInternalChainStart->push(*this, iteration, &startMsg))
		{
			error = true;
			errorComponent = m_pInternalChainStart->getComponentName();
			errorString = m_pInternalChainStart->getErrorString();
			m_pInternalChainStart->unlock();
			m_chainMutex.Unlock();
			break;
		}
#ifdef MIPDEBUG2
		std::cout << m_chainName << " push stop:  " << m_pInternalChainStart->getComponentName() << std::endl;
#endif // MIPDEBUG2
		m_pInternalChainStart->unlock();

#ifdef MIPDEBUG
	//	MIPTime curTime = MIPTime::getCurrentTime();
#endif // MIPDEBUG
		
		std::list<MIPConnection>::const_iterator it;

		for (it = m_orderedConnections.begin() ; !error && it != m_orderedConnections.end() ; it++)
		{
			MIPComponent *pPullComp = (*it).getPullComponent();
			MIPComponent *pPushComp = (*it).getPushComponent();
			uint32_t mask1 = (*it).getMask1();
			uint32_t mask2 = (*it).getMask2();

			pPullComp->lock();
			if (pPushComp->getComponentPointer() != pPullComp->getComponentPointer())
				pPushComp->lock();

			MIPMessage *msg = 0;
#ifdef MIPDEBUG3
			int msgCount = 0;
#endif // MIPDEBUG3
			do
			{
#ifdef MIPDEBUG2
				std::cout << m_chainName << " pull start: " << pPullComp->getComponentName() << std::endl;
#endif // MIPDEBUG2
				if (!pPullComp->pull(*this, iteration, &msg))
				{
					error = true;
					errorComponent = pPullComp->getComponentName();
					errorString = pPullComp->getErrorString();
				}
				else
				{
#ifdef MIPDEBUG2
					std::cout << m_chainName << " pull stop:  " << pPullComp->getComponentName() << std::endl;
#endif // MIPDEBUG2
					if (msg) // Ok, pass the message
					{
						uint32_t msgType = msg->getMessageType();
						uint32_t msgSubtype = msg->getMessageSubtype();

						if ((msgType&mask1) && (msgSubtype&mask2))
						{
#ifdef MIPDEBUG3
							msgCount++;
#endif // MIPDEBUG3

#ifdef MIPDEBUG2
							std::cout << m_chainName << " push start: " << pPushComp->getComponentName() << std::endl;
#endif // MIPDEBUG2
							if(!pPushComp->push(*this, iteration, msg))
							{
								error = true;
								errorComponent = pPushComp->getComponentName();
								errorString = pPushComp->getErrorString();
							}
#ifdef MIPDEBUG2
							std::cout << m_chainName << " push stop:  " << pPushComp->getComponentName() << std::endl;
#endif // MIPDEBUG2
						}
					}
#ifdef MIPDEBUG2
					std::cout << m_chainName << " all messages pushed" << std::endl;
#endif // MIPDEBUG2

				}
			} while (!error && msg);
			
			pPullComp->unlock();
			if (pPushComp->getComponentPointer() != pPullComp->getComponentPointer())
				pPushComp->unlock();
#ifdef MIPDEBUG3
			std::cout << "   Transferred " << msgCount << " messages from " << pPullComp->getComponentName() << " (" << (void *)pPullComp << ") to " << pPushComp->getComponentName() << " (" << (void  *)pPushComp << ")" << std::endl;
#endif // MIPDEBUG3
		}

		if (error)
		{
			m_chainMutex.Unlock();
			break;
		}
		
		std::list<MIPComponent *>::const_iterator fbIt;
		MIPFeedback feedback;
		int64_t chainID = 0;
		
#ifdef MIPDEBUG4
		//std::cerr << "Processing feedback:" << std::endl;
#endif // MIPDEBUG4
		
#ifdef MIPDEBUG4
		std::cerr << "\tNEW CHAIN" << std::endl;
#endif // MIPDEBUG4

		for (fbIt = m_feedbackChain.begin() ; !error && fbIt != m_feedbackChain.end() ; fbIt++)
		{
			if ((*fbIt) == 0)
			{
				feedback = MIPFeedback(); // reinitialize feedback
				chainID++;
#ifdef MIPDEBUG4
				std::cerr << "\tNEW CHAIN" << std::endl;
#endif // MIPDEBUG4
			}
			else
			{
				MIPComponent *pFbComp = *fbIt;
				pFbComp->lock();
#ifdef MIPDEBUG4
				std::cerr << "\t\t" << pFbComp->getComponentName() << " " << ((void *)pFbComp) << std::endl;
#endif // MIPDEBUG4
				if (!pFbComp->processFeedback(*this, chainID, &feedback))
				{
					error = true;
					errorComponent = pFbComp->getComponentName();
					errorString = pFbComp->getErrorString();
				}
				pFbComp->unlock();
			}
		}

		m_chainMutex.Unlock();
		
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
	
#ifdef MIPDEBUG
	std::cout << "MIPComponentChain::Thread stopped" << std::endl;
#endif // MIPDEBUG
	
	return 0;
}

bool MIPComponentChain::orderConnections(std::list<MIPConnection> &orderedConnections)
{
	std::list<MIPConnection> orderedList;
	std::list<MIPConnection>::iterator it;
	std::list<MIPComponent *> componentLayer;

	for (it = m_inputConnections.begin() ; it != m_inputConnections.end() ; it++)
		(*it).setMark(false);

#ifdef MIPDEBUG3
	int layerNumber = 0;
	std::cout << "Start of connection ordering" << std::endl;
#endif // MIPDEBUG3
	
	componentLayer.push_back(m_pInputChainStart);
	while (!componentLayer.empty())
	{
#ifdef MIPDEBUG3
		layerNumber++;
		std::cout << "Layer " << layerNumber << ":" << std::endl;
#endif // MIPDEBUG3
		std::list<MIPComponent *> newLayer;
		std::list<MIPComponent *>::const_iterator compit;
		
		for (compit = componentLayer.begin() ; compit != componentLayer.end() ; compit++)
		{
			for (it = m_inputConnections.begin() ; it != m_inputConnections.end() ; it++)
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
#ifdef MIPDEBUG3
						std::cout << "   " << (*it).getPullComponent()->getComponentName() << " (" << (void *)((*it).getPullComponent()) << ") -> " << (*it).getPushComponent()->getComponentName() << " (" << (void*)((*it).getPushComponent()) << ")" <<std::endl;
#endif // MIPDEBUG3

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
	
#ifdef MIPDEBUG3
	std::cout << "End of connection ordering" << std::endl;
#endif // MIPDEBUG3

	for (it = m_inputConnections.begin() ; it != m_inputConnections.end() ; it++)
	{
		if (!(*it).isMarked())
		{
			setErrorString(MIPCOMPONENTCHAIN_ERRSTR_UNUSEDCONNECTION);
			return false;		
		}
	}

	orderedConnections = orderedList;
	
	return true;
}

bool MIPComponentChain::buildFeedbackList(std::list<MIPConnection> &orderedList, std::list<MIPComponent *> &feedbackComponentChain)
{
	std::list<MIPConnection>::iterator it;
	std::list<MIPComponent *> subChain;
	std::list<MIPComponent *> feedbackChain;
	bool done = false;

	for (it = orderedList.begin() ; it != orderedList.end() ; it++)
	{
		if (!(*it).giveFeedback())
			(*it).setMark(true);
		else
			(*it).setMark(false);
	}
	
	while (!done)
	{
		bool found = false;

		it = orderedList.begin();
		while (!found && it != orderedList.end())
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
				while (it != orderedList.end())
				{
					// The check for isMarked causes some problems when a feedback chain splits
					// into two parts. By omitting this check, two separate chains should be
					// created, which both have a common part
					if (/*!(*it).isMarked() &&*/ (*it).giveFeedback())
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
		
			if (!feedbackChain.empty())
				feedbackChain.push_front(0); // mark new subchain

			std::list<MIPComponent *>::const_iterator it2;

			for (it2 = subChain.begin() ; it2 != subChain.end() ; it2++)
				feedbackChain.push_front(*it2);
		}
		else
			done = true;
	}

	feedbackComponentChain = feedbackChain;
	
	return true;
}

void MIPComponentChain::copyConnectionInfo(const std::list<MIPConnection> &orderedList, const std::list<MIPComponent *> &feedbackChain)
{
	std::list<MIPConnection>::const_iterator it;
	std::list<MIPComponent *>::const_iterator it2;

	m_chainMutex.Lock();
	
	m_orderedConnections.clear();
	m_feedbackChain.clear();
	
	for (it = orderedList.begin() ; it != orderedList.end() ; it++)
		m_orderedConnections.push_back(*it);
	for (it2 = feedbackChain.begin() ; it2 != feedbackChain.end() ; it2++)
		m_feedbackChain.push_back(*it2);
	
	m_pInternalChainStart = m_pInputChainStart;

	m_chainMutex.Unlock();
}	
