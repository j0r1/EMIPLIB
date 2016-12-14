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
 * \file miprtpvideodecoder.h
 */

#ifndef MIPRTPVIDEODECODER_H

#define MIPRTPVIDEODECODER_H

#include "mipconfig.h"
#include "miprtppacketdecoder.h"
#include "miprtppacketgrouper.h"
#include "miptime.h"
#if defined(WIN32) || defined(_WIN32_WCE)
	#include <hash_map>
#else
	#include <ext/hash_map>
#endif // Win32

/** This class decodes incoming RTP data into video messages.
 *  This class takes MIPRTPReceiveMessages as input and generates 
 *  video messages. The RTP messages themselves should use the
 *  internal format provided by MIPRTPVideoEncoder.
 */
class MIPRTPVideoDecoder : public MIPRTPPacketDecoder
{
public:
	MIPRTPVideoDecoder();
	~MIPRTPVideoDecoder();
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

#if defined(WIN32) || defined(_WIN32_WCE)
	stdext::hash_map<uint32_t, PacketGrouper *> m_packetGroupers;
#else
	__gnu_cxx::hash_map<uint32_t, PacketGrouper * > m_packetGroupers;
#endif // Win32
	MIPTime m_lastCheckTime;
};

#endif // MIPRTPVIDEODECODER_H

