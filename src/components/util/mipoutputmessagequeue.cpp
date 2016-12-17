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

#include "mipdebug.h"

#define MIPOUTPUTMESSAGEQUEUE_ERRSTR_STATEEXISTS				"A state is already present for this source ID"
#define MIPOUTPUTMESSAGEQUEUE_ERRSTR_STATEZERO					"The specified state is null, which is not allowed"

MIPOutputMessageQueue::MIPOutputMessageQueue(const std::string &componentName) : MIPComponent(componentName)
{
	m_prevIteration = -1;
}

MIPOutputMessageQueue::~MIPOutputMessageQueue()
{
	clearMessages();
}

void MIPOutputMessageQueue::init()
{
	clearMessages();
	m_prevIteration = -1;
}

void MIPOutputMessageQueue::checkIteration(int64_t iteration)
{
	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}
}

bool MIPOutputMessageQueue::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
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

