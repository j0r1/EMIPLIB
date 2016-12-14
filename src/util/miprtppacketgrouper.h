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
 * \file miprtppacketgrouper.h
 */

#ifndef MIPRTPPACKETGROUPER_H

#define MIPRTPPACKETGROUPER_H

#include "mipconfig.h"
#include "miperrorbase.h"
#include "miptypes.h"
#include <vector>
#include <list>

namespace jrtplib
{
	class RTPPacket;
}

/** A helper class to help restore data if it had to be split across
 *  several RTP packets.
 *  This is a helper class to help reassemble a packet which had to
 *  be split across multiple RTP packets. Basically, it stores 
 *  packets until all packets with the same RTP timestamp have
 *  been received. It is assumed that the RTP marker bit signals
 *  the last RTP packet with a specific timestamp, but it can
 *  also work if the marker bit is never set. In this case however,
 *  an extra packet (with a new timestamp) will need to be received 
 *  to be sure that all packets with a specific timestamp have been
 *  processed.
 */
class MIPRTPPacketGrouper : public MIPErrorBase
{
public:
	MIPRTPPacketGrouper();
	~MIPRTPPacketGrouper();

	/** Initializes the packet grouper.
	 *  Initializes the packet grouper.
	 *  \param ssrc Only RTP packet with this ssrc will be processed.
	 *  \param bufSize Number of consecutive sequence numbers for which RTP packets
	 *                 can be held in memory.
	 */
	bool init(uint32_t ssrc, int bufSize = 128);

	/** De-initializes the packet grouper. */
	void clear();

	/** Process a new RTP packet.
	 *  Process a new RTP packet. If by some means one can be sure that this packet
	 *  is the first part of a set of fragments which all have the same timestamp,
	 *  the \c isFirstFramePart can be set, but it is not absolutely necessary for
	 *  the packet grouper to work correctly.
	 */
	bool processPacket(const jrtplib::RTPPacket *pPack, bool isFirstFramePart);

	/** Extract queued message parts which correspond to the same timestamp.
	 *  Extract queued message parts which correspond to the same timestamp. If no
	 *  such parts are available, the lists will be empty.
	 *  \param parts A list of pointers to the payloads of all the RTP packets that belong
	 *               to a specific sampling instant.
	 *  \param partSizes A corresponding list of sizes of these payloads.
	 *  \param timestamp Specifies the common RTP timestamp of all these fragments.
	 */
	void getNextQueuedPacket(std::vector<uint8_t *> &parts, std::vector<size_t> &partSizes, uint32_t &timestamp);
private:
	//void printTable();
	void checkCompletePacket(int index, std::vector<uint8_t *> &parts, std::vector<size_t> &partSizes, uint32_t &packetTimestamp);
	void checkMaximumQueueLength();

	std::vector<uint32_t> m_extendedSequenceNumbers;
	std::vector<uint32_t> m_timestamps;
	std::vector<bool> m_skipFlags;
	std::vector<uint8_t *> m_packetParts;
	std::vector<size_t> m_partSizes;
	std::vector<bool> m_markers;
	std::vector<bool> m_firstPartMarkers;
	int m_startPosition;
	uint32_t m_ssrc;

	std::list<std::vector<uint8_t *> > m_partQueue;
	std::list<std::vector<size_t> > m_partSizeQueue;
	std::list<uint32_t> m_timestampQueue;
};

#endif // MIPRTPPACKETGROUPER_H
