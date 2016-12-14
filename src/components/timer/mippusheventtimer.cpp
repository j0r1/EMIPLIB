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

#include "mipconfig.h"
#include "mippusheventtimer.h"

MIPPushEventTimer::MIPPushEventTimer() : MIPComponent("MIPPushEventTimer"),
                                         m_timeMsg(MIPSYSTEMMESSAGE_TYPE_ISTIME)
{
	m_prevIteration = -1;
}

MIPPushEventTimer::~MIPPushEventTimer()
{
}

bool MIPPushEventTimer::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	m_prevIteration = iteration;
	m_gotMsg = false;
	return true;
}

bool MIPPushEventTimer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
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

