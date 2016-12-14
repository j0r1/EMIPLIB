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
 * \file mipaveragetimer.h
 */

#ifndef MIPAVERAGETIMER_H

#define MIPAVERAGETIMER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipsystemmessage.h"

class MIPComponentChain;

/** A simple timing component.
 *  This is a simple timing component which accepts MIPSYSTEMMESSAGE_WAITTIME system
 *  messages. It generates a MIPSYSTEMMESSAGE_ISTIME system message each time the
 *  specified interval has elapsed. Note that this is only on average after each interval:
 *  fluctuation will be present.
 */
class MIPAverageTimer : public MIPComponent
{
public:
	/** Creates a timing object.
	 *  Using this constructor, a timing object will be created which will generate
	 *  messages eacht time \c interval has elapsed.
	 */
	MIPAverageTimer(MIPTime interval);
	~MIPAverageTimer();

	/** Re-initializes the component. */
	void reset();
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	const MIPComponentChain *m_pChain;
	MIPTime m_startTime, m_interval;
	MIPSystemMessage m_timeMsg;
	bool m_gotMsg;
};

#endif // MIPAVERAGETIMER_H

