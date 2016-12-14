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
 * \file miprtpcomponent.h
 */

#ifndef MIPRTPCOMPONENT_H

#define MIPRTPCOMPONENT_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include <list>

class MIPRTPReceiveMessage;

namespace jrtplib
{
	class RTPSession;
	class RTPSourceData;
	class RTPPacket;
}

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
	 *  \param silentTimestampIncrement When using some kind of silence suppression or push-to-talk
	 *                                  system, it is possible that during certain intervals no
	 *                                  messages will reach this component. For these 'skipped'
	 *                                  intervals, the RTP timestamp will be increased by this amount.
	 */
	bool init(jrtplib::RTPSession *pSess, uint32_t silentTimestampIncrement = 0);

	/** De-initializes the component. */
	bool destroy();

	/** See the MIPRTPComponent::init function for the meaning of this parameter. */
	void setSilentSampleIncrement(uint32_t silentTimestampIncrement)					{ m_silentTimestampIncrease = silentTimestampIncrement; }

	/** This flag controls if RTP packets are actually sent out, useful for a push-to-talk system for example (enabled by default). */
	void setEnableSending(bool f)										{ m_enableSending = f; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
protected:
	/** Returns the source ID for the packet \c pPack belonging to source \c pSourceData.
	 *  This virtual function returns the source ID when processing packet \c pPack originating
	 *  from source \c pSourceData. By default, the 32-bit SSRC value is cast to a 64-bit value
	 *  and is returned. In case this behaviour is not desirable, you can derive your own class
	 *  from MIPRTPComponent and you can re-implement this function.
	 */
	virtual uint64_t getSourceID(const jrtplib::RTPPacket *pPack, const jrtplib::RTPSourceData *pSourceData) const;
private:
	bool processNewPackets(int64_t iteration);	
	void clearMessages();
	
	std::list<MIPRTPReceiveMessage *> m_messages;
	std::list<MIPRTPReceiveMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
	int64_t m_prevSendIteration;
	jrtplib::RTPSession *m_pRTPSession;
	uint32_t m_silentTimestampIncrease;
	bool m_enableSending;
};

#endif // MIPRTPCOMPONENT_H
