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

#include "mipconfig.h"
#include "miprtph263decoder.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"
#include "miprtpmessage.h"
#include <rtppacket.h>

#include "mipdebug.h"

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

MIPRTPH263Decoder::MIPRTPH263Decoder()
{
}

MIPRTPH263Decoder::~MIPRTPH263Decoder()
{
	hash_map<uint32_t, PacketGrouper *>::iterator it;

	for (it = m_packetGroupers.begin() ; it != m_packetGroupers.end() ; it++)
		delete (*it).second;
	m_packetGroupers.clear();
}

bool MIPRTPH263Decoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit)
{
	size_t length;
	const uint8_t *pPayload = pRTPPack->GetPayloadData();

	if (!(pPayload[1] == 0x00 && (pPayload[0] == 0x00 || pPayload[0] == 0x04)))
		return false;
	
	if ((length = (size_t)pRTPPack->GetPayloadLength()) <= 3)
		return false;

	timestampUnit = 1.0/90000.0;
	
	return true;
}

void MIPRTPH263Decoder::createNewMessages(const RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps)
{
	const uint8_t *pPayload = pRTPPack->GetPayloadData();
	bool firstFramePart = false;

	expireGroupers();

	hash_map<uint32_t, PacketGrouper *>::iterator it;
	uint32_t ssrc = pRTPPack->GetSSRC();
	MIPRTPPacketGrouper *pGrouper = 0;

	it = m_packetGroupers.find(ssrc);
	if (it == m_packetGroupers.end()) // no entry exists yet
	{
		pGrouper = new MIPRTPPacketGrouper();

		if (!pGrouper->init(ssrc))
		{
			// TODO: report error somehow?
			delete pGrouper;
			return;
		}

		m_packetGroupers[ssrc] = new PacketGrouper(pGrouper);
	}
	else
		pGrouper = (*it).second->getGrouper();


	if (!pGrouper->processPacket(pRTPPack, firstFramePart)) // TODO: error reporting?
		return;

	bool done = false;

	while (!done)
	{
		std::vector<uint8_t *> parts;
		std::vector<size_t> sizes;
		uint32_t timestamp;

		pGrouper->getNextQueuedPacket(parts, sizes, timestamp);

		if (parts.size() == 0)
			done = true;
		else
		{
			size_t totalSize = 0;
			bool valid = true;

			for (int i = 0 ; valid && i < sizes.size() ; i++)
			{
				if (sizes[i] < 2) // shouldn't happen!
					valid = false;

				if (parts[i][0] == 0x04)
					totalSize += sizes[i];
				else
					totalSize += (sizes[i]-2);
			}

			if (valid)
			{
				uint8_t *pData = new uint8_t [totalSize];
				size_t offset = 0;

				for (int i = 0 ; i < parts.size() ; i++)
				{
					if (parts[i][0] == 0x04)
					{
						memcpy(pData + offset, parts[i], sizes[i]);
						pData[offset+0] = 0x00;
						pData[offset+1] = 0x00;
						offset += sizes[i];
					}
					else
					{
						memcpy(pData + offset, parts[i]+2, sizes[i]-2);
						offset += (sizes[i]-2);
					}
				}

				if (pData[0] == 0x00 && pData[1] == 0x00)
				{
					MIPEncodedVideoMessage *pVidMsg = new MIPEncodedVideoMessage(MIPENCODEDVIDEOMESSAGE_TYPE_H263P, 0, 0, pData, totalSize, true);
				
					messages.push_back(pVidMsg);
					timestamps.push_back(timestamp);
				}
				else
					delete [] pData;
			}

			for (int i = 0 ; i < parts.size() ; i++)
				delete [] parts[i];
		}
	}
}

void MIPRTPH263Decoder::expireGroupers()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastCheckTime.getValue()) < 10.0)
		return;

	m_lastCheckTime = curTime;

	hash_map<uint32_t, PacketGrouper *>::iterator it;

	it = m_packetGroupers.begin(); 

	while (it != m_packetGroupers.end())
	{
		PacketGrouper *pPackGroup = (*it).second;

		if (pPackGroup->getLastAccessTime().getValue() - curTime.getValue() > 10.0) // TODO: make this configurable?
		{
			hash_map<uint32_t, PacketGrouper *>::iterator it2 = it;

			it++;

			delete (*it2).second;
			m_packetGroupers.erase(it2);
		}
		else
			it++;
	}
}
