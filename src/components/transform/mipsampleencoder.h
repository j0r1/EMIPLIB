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
 * \file mipsampleencoder.h
 */

#ifndef MIPSAMPLEENCODER_H

#define MIPSAMPLEENCODER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miprawaudiomessage.h"
#include <list>

class MIPAudioMessage;

/** Changes the sample encoding of raw audio messages.
 *  This component can be used to change the sample encoding of raw audio messages.
 *  It accepts all raw audio messages and produces similar raw audio messages, using
 *  a predefined encoding type.
 */
class EMIPLIB_IMPORTEXPORT MIPSampleEncoder : public MIPComponent
{
public:
	MIPSampleEncoder();
	~MIPSampleEncoder();

	/** Initializes the sample encoder.
	 *  Using this function, the sample encoder is initialized. Messages produced by
	 *  this component will be raw audio messages of subtype \c destType. See miprawaudiomessage.h
	 *  for an overview of the subtypes.
	 */
	bool init(int destType);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();

	bool m_init;
	int m_dstType;
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPSAMPLEENCODER_H

