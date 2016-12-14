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
 * \file mipaudiosplitter.h
 */

#ifndef MIPAUDIOSPLITTER_H

#define MIPAUDIOSPLITTER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>

class MIPAudioMessage;

/** Split incoming raw audio messages into smaller messages.
 *  This component takes raw audio messages as its input and produces similar
 *  messages, containing at most a specified amount of data.
 */
class EMIPLIB_IMPORTEXPORT MIPAudioSplitter : public MIPComponent
{
public:
	MIPAudioSplitter();
	~MIPAudioSplitter();

	/** Initializes the component.
	 *  This function (re)initializes the component.
	 *  \param interval The messages sent to the output will contain the amount of
	 *                  data corresponding to this parameter.
	 */
	bool init(MIPTime interval);
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();

	bool m_init;
	MIPTime m_interval;
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPAUDIOSPLITTER_H

