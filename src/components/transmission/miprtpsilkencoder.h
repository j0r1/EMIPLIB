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
 * \file miprtpsilkencoder.h
 */

#ifndef MIPRTPSILKENCODER_H

#define MIPRTPSILKENCODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SILK

#include "miprtpencoder.h"
#include <list>

class MIPRTPSendMessage;

/** Creates RTP packets for SILK compressed audio packets.
 *  This component accepts incoming SILK compressed audio packets and generates 
 *  MIPRTPSendMessage objects which can then be transferred to a MIPRTPComponent instance.
 */
class EMIPLIB_IMPORTEXPORT MIPRTPSILKEncoder : public MIPRTPEncoder
{
public:
	MIPRTPSILKEncoder();
	~MIPRTPSILKEncoder();

	/** Initializes the encoder.
	 *  Initializes the component.
	 *  \param timestampsPerSecond Tells the component how much the RTP timestamp should be increased
	 *                             each second. This can be either 8000, 12000, 16000 or 24000.
	 */
	bool init(int timestampsPerSecond);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();

	bool m_init;
	int m_timestampsPerSecond;
	std::list<MIPRTPSendMessage *> m_messages;
	std::list<MIPRTPSendMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPCONFIG_SUPPORT_SILK

#endif // MIPRTPSILKENCODER_H

