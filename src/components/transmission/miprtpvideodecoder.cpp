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
#include "miprtpvideodecoder.h"
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

MIPRTPVideoDecoder::MIPRTPVideoDecoder()
{
}

MIPRTPVideoDecoder::~MIPRTPVideoDecoder()
{
	hash_map<uint32_t, PacketGrouper *>::iterator it;

	for (it = m_packetGroupers.begin() ; it != m_packetGroupers.end() ; it++)
		delete (*it).second;
	m_packetGroupers.clear();
}

bool MIPRTPVideoDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit)
{
	size_t length;
	const uint8_t *pPayload = pRTPPack->GetPayloadData();

	if ((length = (size_t)pRTPPack->GetPayloadLength()) <= 1)
		return false;

	if (pPayload[0] != 0xff)
	{
		if (length <= 9)
			return false;
	}

	timestampUnit = 1.0/90000.0;
	
	return true;
}

void MIPRTPVideoDecoder::createNewMessages(const RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps)
{
	const uint8_t *pPayload = pRTPPack->GetPayloadData();
	bool firstFramePart = true;

	if (pPayload[0] == 0xff)
		firstFramePart = false;

	expireGroupers();

	hash_map<uint32_t, PacketGrouper *>::iterator it;
	uint32_t ssrc = pRTPPack->GetSSRC();
	MIPRTPPacketGrouper *pGrouper = 0;

	it = m_packetGroupers.find(ssrc);
	if (it == m_packetGroupers.end()) // no entry exists yet
	{
		pGrouper = new MIPRTPPacketGrouper();

		if (!pGrouper->init(ssrc, 1024))
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
			bool valid = true;

			if (sizes[0] <= 9) // not a valid packet, skip it
				valid = false;

			if (valid) // ok, packet seems valid so far
			{
				size_t totalSize = sizes[0] - 9;

				for (int i = 1 ; valid && i < sizes.size() ; i++)
				{
					if (sizes[i] < 2) // shouldn't happen!
						valid = false;

					totalSize += (sizes[i]-1);
				}

				if (valid)
				{
					uint8_t *pData = new uint8_t [totalSize];
					size_t offset = sizes[0] - 9;

					pPayload = parts[0];

					uint32_t width = 0;
					uint32_t height = 0;
					uint8_t typeByte = pPayload[0];
						
					width = (uint32_t)pPayload[1];
					width |= ((uint32_t)pPayload[2])<<8;
					width |= ((uint32_t)pPayload[3])<<16;
					width |= ((uint32_t)pPayload[4])<<24;
					height = (uint32_t)pPayload[5];
					height |= ((uint32_t)pPayload[6])<<8;
					height |= ((uint32_t)pPayload[7])<<16;
					height |= ((uint32_t)pPayload[8])<<24;

					memcpy(pData, parts[0] + 9, sizes[0] - 9);

					for (int i = 1 ; i < parts.size() ; i++)
					{
						memcpy(pData + offset, parts[i]+1, sizes[i]-1);
						offset += (sizes[i]-1);
					}

					MIPVideoMessage *pVidMsg = 0;

					if (typeByte == 0x00) // YUV420P
					{
						if ((uint32_t)totalSize != (width*height*3)/2)
						{
							// invalid packet
							// TODO: error reporting?
						}
						else
							pVidMsg = new MIPRawYUV420PVideoMessage((int)width, (int)height, pData, true);
					}
					else // must be H.263 for now
						pVidMsg = new MIPEncodedVideoMessage(MIPENCODEDVIDEOMESSAGE_TYPE_H263P, (int)width, (int)height, pData, totalSize, true);

					if (pVidMsg)
					{
						messages.push_back(pVidMsg);
						timestamps.push_back(timestamp);
					}
				}
			}

			for (int i = 0 ; i < parts.size() ; i++)
				delete [] parts[i];
		}
	}
}

void MIPRTPVideoDecoder::expireGroupers()
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

