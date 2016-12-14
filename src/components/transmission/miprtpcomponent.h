/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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
 * \file miprtpcomponent.h
 */

#ifndef MIPRTPCOMPONENT_H

#define MIPRTPCOMPONENT_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include <list>

class MIPRTPReceiveMessage;
class RTPSession;
class RTPSourceData;
class RTPPacket;

/** An RTP transmission component which uses an RTPSession object (from the \c JRTPLIB library).
 *  This component can transmit and receive RTP messages. It uses the \c JRTPLIB 
 *  \c RTPSession class to accomplish this. The RTPSession instance needs to be initialized before 
 *  the component can be used. Both MIPSYSTEMMESSAGE_TYPE_ISTIME messages and messages of type 
 *  MIPRTPSendMessage are accepted. Messages produced by this component are of type 
 *  MIPRTPReceiveMessage.
 */
class MIPRTPComponent : public MIPComponent
{
public:
	MIPRTPComponent();
	~MIPRTPComponent();

	/** Initializes the component.
	 *  With this function the component can be initialized.
	 *  \param pSess The JRTPLIB RTPSession object which will be used to receive and transmit
	 *               RTP packets.
	 */
	bool init(RTPSession *pSess);

	/** De-initializes the component. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
protected:
	/** Returns the source ID for the packet \c pPack belonging to source \c pSourceData.
	 *  This virtual function returns the source ID when processing packet \c pPack originating
	 *  from source \c pSourceData. By default, the 32-bit SSRC value is cast to a 64-bit value
	 *  and is returned. In case this behaviour is not desirable, you can derive your own class
	 *  from MIPRTPComponent and you can re-implement this function.
	 */
	virtual uint64_t getSourceID(const RTPPacket *pPack, const RTPSourceData *pSourceData) const;
private:
	bool processNewPackets(int64_t iteration);	
	void clearMessages();
	
	std::list<MIPRTPReceiveMessage *> m_messages;
	std::list<MIPRTPReceiveMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
	RTPSession *m_pRTPSession;
};

#endif // MIPRTPCOMPONENT_H
