/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Hasselt University - Expertise Centre for
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
#include "mipaveragetimer.h"
#include "mipsystemmessage.h"
//#include <iostream>

#include "mipdebug.h"

#define MIPAVERAGETIMER_ERRSTR_WRONGCHAIN		"Already in use by another component chain"
#define MIPAVERAGETIMER_ERRSTR_BADMESSAGE		"Only a WAITTIME message is supported"

MIPAverageTimer::MIPAverageTimer(MIPTime interval) : MIPComponent("MIPAverageTimer"),
						     m_startTime(0),
						     m_interval(interval),
						     m_timeMsg(MIPSYSTEMMESSAGE_TYPE_ISTIME)
{
	reset();
}

MIPAverageTimer::~MIPAverageTimer()
{
}

void MIPAverageTimer::reset()
{
	m_pChain = 0;
	m_gotMsg = false;
}

bool MIPAverageTimer::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pChain == 0)
	{
		m_startTime = MIPTime::getCurrentTime();
		m_pChain = &chain;	
	}
	else
	{
		if (m_pChain != &chain)
		{
			setErrorString(MIPAVERAGETIMER_ERRSTR_WRONGCHAIN);
			return false;
		}
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME))
	{
		setErrorString(MIPAVERAGETIMER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPTime curTime = MIPTime::getCurrentTime();
	real_t diff = (m_startTime.getValue()+((real_t)iteration)*m_interval.getValue())-curTime.getValue();
//	std::cout << "Current time: " << curtime.getString() << std::endl;
//	std::cout << "Starttime time: " << mStartTime.getString() << std::endl;
//	std::cout << "Iteration: " << iteration << std::endl;
//	std::cout << "Interval: " << mInterval.getString() << std::endl;
//	std::cout << "diff: " << diff << std::endl;
	
	if (diff > 0)
		MIPTime::wait(MIPTime(diff));
	
	m_gotMsg = false;
	return true;
}

bool MIPAverageTimer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pChain != &chain)
	{
		setErrorString(MIPAVERAGETIMER_ERRSTR_WRONGCHAIN);
		return false;
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

