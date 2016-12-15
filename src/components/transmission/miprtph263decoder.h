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
 * \file miprtph263decoder.h
 */

#ifndef MIPRTPH263DECODER_H

#define MIPRTPH263DECODER_H

#include "mipconfig.h"
#include "miprtppacketdecoder.h"
#include "miprtppacketgrouper.h"
#include "miptime.h"
#include <unordered_map>

/** This class decodes incoming RTP data into H.263 video messages.
 *  This class takes MIPRTPReceiveMessages as input and generates 
 *  H.263 video messages. 
 */
class EMIPLIB_IMPORTEXPORT MIPRTPH263Decoder : public MIPRTPPacketDecoder
{
public:
	MIPRTPH263Decoder();
	~MIPRTPH263Decoder();
private:
	bool validatePacket(const jrtplib::RTPPacket *pRTPPack, real_t &timestampUnit, real_t timestampUnitEstimate);
	void createNewMessages(const jrtplib::RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps);

	void expireGroupers();

	class PacketGrouper
	{
	public:
		PacketGrouper(MIPRTPPacketGrouper *pPacketGrouper)
		{
			m_pGrouper = pPacketGrouper;
			m_lastAccesstime = MIPTime::getCurrentTime();
		}

		~PacketGrouper()
		{
			delete m_pGrouper;
		}

		MIPRTPPacketGrouper *getGrouper()
		{
			m_lastAccesstime = MIPTime::getCurrentTime();
			return m_pGrouper;
		}

		MIPTime getLastAccessTime() const
		{
			return m_lastAccesstime;
		}
	private:
		MIPTime m_lastAccesstime;
		MIPRTPPacketGrouper *m_pGrouper;
	};

	std::unordered_map<uint32_t, PacketGrouper *> m_packetGroupers;
	MIPTime m_lastCheckTime;
};

#endif // MIPRTPH263DECODER_H

