/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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
 * \file mippusheventtimer.h
 */

#ifndef MIPPUSHEVENTTIMER_H

#define MIPPUSHEVENTTIMER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "mipsystemmessage.h"

/** Component which generates a MIPSYSTEMMESSAGE_ISTIME system message when data has
 *  been supplied to it during this iteration.
 *  This component generates a MIPSYSTEMMESSAGE_ISTIME system message when another
 *  component has delivered data to it during this iteration. It accepts any incoming
 *  message.
 *
 *  Suppose you have a soundcard input component at the start of the chain.
 *  You can then add a link from the soundcard input component to this component
 *  to make this component behave like it's a timing component. 
 */
class MIPPushEventTimer : public MIPComponent
{
public:
	MIPPushEventTimer();
	~MIPPushEventTimer();
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	MIPSystemMessage m_timeMsg;
	bool m_gotMsg;
	int64_t m_prevIteration;
};

#endif // MIPPUSHEVENTTIMER_H
