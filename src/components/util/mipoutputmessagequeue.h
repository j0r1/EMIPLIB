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
 * \file mipoutputmessagequeue.h
 */

#ifndef MIPOUTPUTMESSAGEQUEUE_H

#define MIPOUTPUTMESSAGEQUEUE_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <unordered_map>
#include <list>

class MIPMessage;

class EMIPLIB_IMPORTEXPORT MIPStateInfo
{
public:
	MIPStateInfo();
	virtual ~MIPStateInfo();

	MIPTime getLastUpdateTime() const							{ return m_lastTime; }
	void setUpdateTime()									{ m_lastTime = MIPTime::getCurrentTime(); }
private:
	MIPTime m_lastTime;
};

class EMIPLIB_IMPORTEXPORT MIPOutputMessageQueue : public MIPComponent
{
protected:
	MIPOutputMessageQueue(const std::string &componentName);
public:
	~MIPOutputMessageQueue();

	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
protected:
	void init();
	void checkIteration(int64_t iteration);
	void clearMessages();
	void addToOutputQueue(MIPMessage *pMsg, bool deleteMessage);

	void setExpirationDelay(real_t delay);
	void clearStates();
	MIPStateInfo *findState(uint64_t id);
	bool addState(uint64_t id, MIPStateInfo *pState);
private:
	void expire();

	std::list<std::pair<MIPMessage *, bool> > m_messages;
	std::list<std::pair<MIPMessage *, bool> >::const_iterator m_msgIt;
	int64_t m_prevIteration;

	bool m_useExpiration;
	double m_expirationDelay;
	MIPTime m_lastExpireTime;
	std::unordered_map<uint64_t, MIPStateInfo *> m_states;
};

#endif // MIPOUTPUTMESSAGEQUEUE_H

