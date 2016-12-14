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

/**
 * \file miprtpmessage.h
 */

#ifndef MIPRTPMESSAGE_H

#define MIPRTPMESSAGE_H

#include "mipconfig.h"
#include "mipmessage.h"
#include "miptime.h"
#include <rtpsession.h>
#include <rtppacket.h>
#include <string.h>

/**
 * \def MIPRTPMESSAGE_TYPE_SEND
 *      \brief This subtype indicates a message of type MIPRTPSendMessage.
 * \def MIPRTPMESSAGE_TYPE_RECEIVE
 *      \brief This subtype indicates a message of type MIPRTPReceiveMessage.
 */

#define MIPRTPMESSAGE_TYPE_SEND										0x00000001
#define MIPRTPMESSAGE_TYPE_RECEIVE									0x00000002

/** This message describes data to be sent using RTP.
 *  This message is a container for data which has to be sent using the RTP protocol.
 *  The subtype of this message is MIPRTPMESSAGE_TYPE_SEND, defined in miprtpmessage.h
 */
class MIPRTPSendMessage : public MIPMessage
{
public:
	/** Constructs a message containing data to be sent using RTP.
	 *  This constructor creates a message with data to be sent using RTP.
	 *  \param pData A pointer to the RTP payload.
	 *  \param dataLength The length of the payload.
	 *  \param payloadType The payload type.
	 *  \param marker Flag indicating if the RTP marker bit should be set.
	 *  \param tsInc The amount with which the timestamp should be incremented.
	 *  \param deleteData Flag indicating if a delete call should be used on \c pData when the
	 *                    message is destroyed.
	 */
	MIPRTPSendMessage(uint8_t *pData, size_t dataLength, uint8_t payloadType, bool marker,
	                  uint32_t tsInc, bool deleteData = true) : MIPMessage(MIPMESSAGE_TYPE_RTP, MIPRTPMESSAGE_TYPE_SEND)
													{ m_pData = pData; m_dataLength = dataLength; m_payloadType = payloadType; m_marker = marker; m_tsInc = tsInc; m_deleteData = deleteData; }
	~MIPRTPSendMessage()										{ if (m_deleteData) delete [] m_pData; }

	/** Returns the payload data. */
	const uint8_t *getPayload() const								{ return m_pData; }

	/** Returns the payload length. */
	size_t getPayloadLength() const									{ return m_dataLength; }

	/** Returns the payload type. */
	uint8_t getPayloadType() const									{ return m_payloadType; }

	/** Returns the marker flag. */
	bool getMarker() const										{ return m_marker; }

	/** Returns the amount with which the timestamp should be incremented. */
	uint32_t getTimestampIncrement() const								{ return m_tsInc; }

	/** Sets the time to which the first sample in the payload corresponds. */
	void setSamplingInstant(MIPTime t)								{ m_samplingInstant = t; }

	/** Returns the sampling instant as set by the MIPRTPSendMessage::setSamplingInstant function. */
	MIPTime getSamplingInstant() const								{ return m_samplingInstant; }
private:
	uint8_t *m_pData;
	size_t m_dataLength;
	uint8_t m_payloadType;
	bool m_marker;
	uint32_t m_tsInc;
	bool m_deleteData;
	MIPTime m_samplingInstant;
};

#define MIPRTPMESSAGE_MAXCNAMELENGTH									256	

/** In this message, data received using the RTP protocol can be stored.
 *  Using this message, received RTP data can be transferred. This class depends on
 *  the \c JRTPLIB \c RTPPacket class.
 */
class MIPRTPReceiveMessage : public MIPMessage
{
public:
	/** Constructs a message containing received RTP data.
	 *  This constructor creates message containing received RTP data.
	 *  \param pPack The received RTP packet.
	 *  \param pCName The CNAME of the source of this packet. Note that this is
	 *                not null-terminated.
	 *  \param cnameLength The length of the CNAME of the source of this packet.
	 *  \param deletePacket Flag indicating if pPack should be deleted when this
	 *                      message is destroyed.
	 *  \param pSess Pointer to the RTPSession instance from which the RTPPacket
	 *               originated. If not null, this RTPSession's DeletePacket
	 *               member function will be used to deallocate the RTPPacket
	 *               memmory.
	 */
	MIPRTPReceiveMessage(RTPPacket *pPack, const uint8_t *pCName, size_t cnameLength, bool deletePacket = true, RTPSession *pSess = 0) : MIPMessage(MIPMESSAGE_TYPE_RTP, MIPRTPMESSAGE_TYPE_RECEIVE), m_jitter(0)
													{ m_deletePacket = deletePacket; m_pPack = pPack; if (cnameLength > MIPRTPMESSAGE_MAXCNAMELENGTH) m_cnameLength = MIPRTPMESSAGE_MAXCNAMELENGTH; else m_cnameLength = cnameLength; if (cnameLength > 0) memcpy(m_cname,pCName,m_cnameLength); m_tsUnit = -1; m_timingInfoSet = false; m_sourceID = 0; m_pSession = pSess; }
	~MIPRTPReceiveMessage()										{ if (m_deletePacket) { if (m_pSession) m_pSession->DeletePacket(m_pPack); else delete m_pPack; } }

	/** Returns the received packet. */
	const RTPPacket *getPacket() const								{ return m_pPack; }

	/** Returns a pointer to the CNAME data of the sender of this packet. */
	const uint8_t *getCName() const									{ return m_cname; }

	/** Returns the length of the CNAME data of the sender of this packet. */
	size_t getCNameLength() const									{ return m_cnameLength; }

	/** Returns the jitter for this source, as set by the MIPRTPReceiveMessage::setJitter function. */
	MIPTime getJitter() const									{ return m_jitter; }

	/** Sets the jitter information for the source of this packet. */
	void setJitter(MIPTime t)									{ m_jitter = t; }

	/** Returns the timestamp unit for the data in the packet, as set by MIPRTPReceiveMessage::setTimestampUnit. */
	real_t getTimestampUnit() const									{ return m_tsUnit; }

	/** Sets the timestamp unit for the data in the packet. */
	void setTimestampUnit(real_t unit)								{ m_tsUnit = unit; }

	/** Returns true if the wallclock time - RTP timestamp relation was set. */
	bool isTimingInfoSet() const									{ return m_timingInfoSet; }

	/** Sets the wallclock time - RTP timestamp relation. */
	void setTimingInfo(MIPTime t, uint32_t timestamp)						{ m_timingInfWallclock = t; m_timingInfTimestamp = timestamp; m_timingInfoSet = true; }
	
	/** If available, the wallclock time - RTP timestamp relation is filled in. */
	bool getTimingInfo(MIPTime *t, uint32_t *timestamp) const					{ if (!m_timingInfoSet) return false; *t = m_timingInfWallclock; *timestamp = m_timingInfTimestamp; return true; }

	/** Stores the source ID which should be used further on. */
	void setSourceID(uint64_t sourceID) 								{ m_sourceID = sourceID; }
	
	/** Returns the source ID which should be used further on. */
	uint64_t getSourceID() const									{ return m_sourceID; }
private:
	RTPPacket *m_pPack;
	RTPSession *m_pSession;
	uint8_t m_cname[MIPRTPMESSAGE_MAXCNAMELENGTH];
	size_t m_cnameLength;
	bool m_deletePacket;
	MIPTime m_jitter;
	real_t m_tsUnit;
	bool m_timingInfoSet;
	MIPTime m_timingInfWallclock;
	uint32_t m_timingInfTimestamp;
	uint64_t m_sourceID;
};

#endif // MIPRTPMESSAGE_H

