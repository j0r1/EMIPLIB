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
#include "mipoutputmessagequeue.h"
#include "mipmessage.h"

#include <iostream> // TODO: for testing

#include "mipdebug.h"

#define MIPOUTPUTMESSAGEQUEUE_ERRSTR_STATEEXISTS				"A state is already present for this source ID"
#define MIPOUTPUTMESSAGEQUEUE_ERRSTR_STATEZERO					"The specified state is null, which is not allowed"

MIPStateInfo::MIPStateInfo()
{
	m_lastTime = MIPTime::getCurrentTime(); 
	//std::cerr << "m_lastTime = " << m_lastTime.getValue() << std::endl;
}

MIPStateInfo::~MIPStateInfo()
{
}

MIPOutputMessageQueue::MIPOutputMessageQueue(const std::string &componentName) : MIPComponent(componentName)
{
	m_prevIteration = -1;
	m_useExpiration = false;
	m_lastExpireTime = MIPTime::getCurrentTime();
	//std::cerr << "m_lastExpireTime = " << m_lastExpireTime.getValue() << std::endl;

}

MIPOutputMessageQueue::~MIPOutputMessageQueue()
{
	clearMessages();
	clearStates();
}

void MIPOutputMessageQueue::init()
{
	clearMessages();
	clearStates();
	m_prevIteration = -1;
	m_useExpiration = false;
	m_lastExpireTime = MIPTime::getCurrentTime();
	//std::cerr << "m_lastExpireTime = " << m_lastExpireTime.getValue() << std::endl;

}

void MIPOutputMessageQueue::checkIteration(int64_t iteration)
{
	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();

		if (m_useExpiration)
			expire();
	}
}

bool MIPOutputMessageQueue::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();

		if (m_useExpiration)
			expire();
	}

	if (m_msgIt == m_messages.end())
	{
		*pMsg = 0;
		m_msgIt = m_messages.begin();
	}
	else
	{
		*pMsg = m_msgIt->first;
		m_msgIt++;
	}

	return true;
}

void MIPOutputMessageQueue::clearMessages()
{
	std::list<std::pair<MIPMessage *, bool > >::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
	{
		if (it->second)
			delete it->first;
	}
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPOutputMessageQueue::addToOutputQueue(MIPMessage *pMsg, bool deleteMessage)
{
	m_messages.push_back(std::pair<MIPMessage *, bool>(pMsg, deleteMessage) );
	m_msgIt = m_messages.begin();
}

void MIPOutputMessageQueue::setExpirationDelay(real_t delay)
{
	//std::cerr << "Setting expiration time to " << delay << std::endl;

	m_useExpiration = true;
	m_expirationDelay = delay;
	m_lastExpireTime = MIPTime::getCurrentTime();

	//std::cerr << "m_lastExpireTime = " << m_lastExpireTime.getValue() << std::endl;

	clearStates();
}

void MIPOutputMessageQueue::clearStates()
{
	//std::cerr << "Clearing all decoder states" << std::endl;

	for (auto it = m_states.begin() ; it != m_states.end() ; it++)
		delete it->second;
	m_states.clear();
}

MIPStateInfo *MIPOutputMessageQueue::findState(uint64_t id)
{
	auto it = m_states.find(id);

	if (it == m_states.end())
	{
		//std::cerr << "Didn't find decoder state for " << id << std::endl;
		return 0;
	}

	//std::cerr << "Found decoder state for " << id << std::endl;

	return it->second;
}

bool MIPOutputMessageQueue::addState(uint64_t id, MIPStateInfo *pState)
{
	if (pState == 0)
	{
		setErrorString(MIPOUTPUTMESSAGEQUEUE_ERRSTR_STATEZERO);
		return false;
	}

	auto it = m_states.find(id);

	if (it != m_states.end())
	{
		setErrorString(MIPOUTPUTMESSAGEQUEUE_ERRSTR_STATEEXISTS);
		return false;
	}
	
	m_states[id] = pState;

	//std::cerr << "Added state for " << id << std::endl;

	return true;
}

void MIPOutputMessageQueue::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	real_t diff = curTime.getValue() - m_lastExpireTime.getValue();

	//std::cout << "diff = " << diff << std::endl;
	//std::cerr << "m_lastExpireTime = " << m_lastExpireTime.getValue() << std::endl;
	//std::cerr << "m_expirationDelay = " << m_expirationDelay << std::endl;


	if (diff < m_expirationDelay)
		return;

	//std::cerr << "Checking expiration times" << std::endl;

	auto it = m_states.begin();
	while (it != m_states.end())
	{
		real_t entryDiff = curTime.getValue() - it->second->getLastUpdateTime().getValue();

		//std::cout << "entryDiff = " << entryDiff << std::endl;

		if (entryDiff > m_expirationDelay)
		{
			auto it2 = it;
			it++;

			//std::cerr << "Expiring " << it2->first << std::endl;
	
			delete it2->second;
			m_states.erase(it2);
		}
		else
		{
			//std::cerr << "Not expiring " << it->first << std::endl;
			it++;
		}
	}
	
	m_lastExpireTime = curTime;
}

