/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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
#include "miprtpjpegdecoder.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"
#include "miprtpmessage.h"
#include <jrtplib3/rtppacket.h>

// TODO: remove this
#include <stdio.h>
#include <iostream>

#include "mipdebug.h"

using namespace jrtplib;

/*
 * Table K.1 from JPEG spec.
 */
static const int jpeg_luma_quantizer[64] = {
        16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68, 109, 103, 77,
        24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
};

/*
 * Table K.2 from JPEG spec.
 */
static const int jpeg_chroma_quantizer[64] = {
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
};

/*
 * Call MakeTables with the Q factor and two uint8_t[64] return arrays
 */
void
MakeTables(int q, uint8_t *lqt, uint8_t *cqt)
{
  int i;
  int factor = q;

  if (q < 1) factor = 1;
  if (q > 99) factor = 99;
  if (q < 50)
    q = 5000 / factor;
  else
    q = 200 - factor*2;

  for (i=0; i < 64; i++) {
    int lq = (jpeg_luma_quantizer[i] * q + 50) / 100;
    int cq = (jpeg_chroma_quantizer[i] * q + 50) / 100;

    /* Limit the quantizers to 1 <= q <= 255 */
    if (lq < 1) lq = 1;
    else if (lq > 255) lq = 255;
    lqt[i] = lq;

    if (cq < 1) cq = 1;
    else if (cq > 255) cq = 255;
    cqt[i] = cq;
  }
}

uint8_t lum_dc_codelens[] = {
        0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
};

uint8_t lum_dc_symbols[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

uint8_t lum_ac_codelens[] = {
        0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d,
};

uint8_t lum_ac_symbols[] = {
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
        0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
        0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
        0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
        0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
        0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa,
};

uint8_t chm_dc_codelens[] = {
        0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

uint8_t chm_dc_symbols[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

uint8_t chm_ac_codelens[] = {
        0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77,
};

uint8_t chm_ac_symbols[] = {
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
        0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
        0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
        0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
        0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa,
};

uint8_t *
MakeQuantHeader(uint8_t *p, uint8_t *qt, int tableNo)
{
        *p++ = 0xff;
        *p++ = 0xdb;            /* DQT */
        *p++ = 0;               /* length msb */
        *p++ = 67;              /* length lsb */
        *p++ = tableNo;
        memcpy(p, qt, 64);
        return (p + 64);
}

uint8_t *
MakeHuffmanHeader(uint8_t *p, uint8_t *codelens, int ncodes,
                  uint8_t *symbols, int nsymbols, int tableNo,
                  int tableClass)
{
        *p++ = 0xff;
        *p++ = 0xc4;            /* DHT */
        *p++ = 0;               /* length msb */
        *p++ = 3 + ncodes + nsymbols; /* length lsb */
        *p++ = (tableClass << 4) | tableNo;
        memcpy(p, codelens, ncodes);
        p += ncodes;
        memcpy(p, symbols, nsymbols);
        p += nsymbols;
        return (p);
}

uint8_t *
MakeDRIHeader(uint8_t *p, uint16_t dri) {
        *p++ = 0xff;
        *p++ = 0xdd;            /* DRI */
        *p++ = 0x0;             /* length msb */
        *p++ = 4;               /* length lsb */
        *p++ = dri >> 8;        /* dri msb */
        *p++ = dri & 0xff;      /* dri lsb */
        return (p);
}

/*
 *  Arguments:
 *    type, width, height: as supplied in RTP/JPEG header
 *    lqt, cqt: quantization tables as either derived from
 *         the Q field using MakeTables() or as specified
 *         in section 4.2.
 *    dri: restart interval in MCUs, or 0 if no restarts.
 *
 *    p: pointer to return area
 *
 *  Return value:
 *    The length of the generated headers.
 *
 *    Generate a frame and scan headers that can be prepended to the
 *    RTP/JPEG data payload to produce a JPEG compressed image in
 *    interchange format (except for possible trailing garbage and
 *    absence of an EOI marker to terminate the scan).
 */
int MakeHeaders(uint8_t *p, int type, int w, int h, uint8_t *lqt,
                uint8_t *cqt, uint16_t dri)
{
        uint8_t *start = p;

        /* convert from blocks to pixels */
        w <<= 3;
        h <<= 3;
        *p++ = 0xff;
        *p++ = 0xd8;            /* SOI */

        p = MakeQuantHeader(p, lqt, 0);
        p = MakeQuantHeader(p, cqt, 1);

        if (dri != 0)
                p = MakeDRIHeader(p, dri);

        *p++ = 0xff;
        *p++ = 0xc0;            /* SOF */
        *p++ = 0;               /* length msb */
        *p++ = 17;              /* length lsb */
        *p++ = 8;               /* 8-bit precision */
        *p++ = h >> 8;          /* height msb */
        *p++ = h;               /* height lsb */
        *p++ = w >> 8;          /* width msb */
        *p++ = w;               /* wudth lsb */
        *p++ = 3;               /* number of components */
        *p++ = 0;               /* comp 0 */
        if (type == 0)
                *p++ = 0x21;    /* hsamp = 2, vsamp = 1 */
        else
                *p++ = 0x22;    /* hsamp = 2, vsamp = 2 */
        *p++ = 0;               /* quant table 0 */
        *p++ = 1;               /* comp 1 */
        *p++ = 0x11;            /* hsamp = 1, vsamp = 1 */
        *p++ = 1;               /* quant table 1 */
        *p++ = 2;               /* comp 2 */
        *p++ = 0x11;            /* hsamp = 1, vsamp = 1 */
        *p++ = 1;               /* quant table 1 */
        p = MakeHuffmanHeader(p, lum_dc_codelens,
                              sizeof(lum_dc_codelens),
                              lum_dc_symbols,
                              sizeof(lum_dc_symbols), 0, 0);
        p = MakeHuffmanHeader(p, lum_ac_codelens,
                              sizeof(lum_ac_codelens),
                              lum_ac_symbols,
                              sizeof(lum_ac_symbols), 0, 1);
        p = MakeHuffmanHeader(p, chm_dc_codelens,
                              sizeof(chm_dc_codelens),
                              chm_dc_symbols,
                              sizeof(chm_dc_symbols), 1, 0);
        p = MakeHuffmanHeader(p, chm_ac_codelens,
                              sizeof(chm_ac_codelens),
                              chm_ac_symbols,
                              sizeof(chm_ac_symbols), 1, 1);

        *p++ = 0xff;
        *p++ = 0xda;            /* SOS */
        *p++ = 0;               /* length msb */
        *p++ = 12;              /* length lsb */
        *p++ = 3;               /* 3 components */
        *p++ = 0;               /* comp 0 */
        *p++ = 0;               /* huffman table 0 */
        *p++ = 1;               /* comp 1 */
        *p++ = 0x11;            /* huffman table 1 */
        *p++ = 2;               /* comp 2 */
        *p++ = 0x11;            /* huffman table 1 */
        *p++ = 0;               /* first DCT coeff */
        *p++ = 63;              /* last DCT coeff */
        *p++ = 0;               /* sucessive approx. */

        return (p - start);
};

MIPRTPJPEGDecoder::MIPRTPJPEGDecoder()
{
}

MIPRTPJPEGDecoder::~MIPRTPJPEGDecoder()
{
	for (auto it = m_packetGroupers.begin() ; it != m_packetGroupers.end() ; it++)
		delete (*it).second;
	m_packetGroupers.clear();
}

bool MIPRTPJPEGDecoder::validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit, real_t timestampUnitEstimate)
{
	size_t length;
	const uint8_t *pPayload = pRTPPack->GetPayloadData();

	if ((length = (size_t)pRTPPack->GetPayloadLength()) < 8)
		return false;

	timestampUnit = 1.0/90000.0;
	
	return true;
}

void MIPRTPJPEGDecoder::createNewMessages(const RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps)
{
	const uint8_t *pPayload = pRTPPack->GetPayloadData();

	expireGroupers();

	uint32_t ssrc = pRTPPack->GetSSRC();
	MIPRTPPacketGrouper *pGrouper = 0;

	auto it = m_packetGroupers.find(ssrc);
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

	bool firstFramePart = false; // TODO: can we set this somehow

	// we've already checked that there are at least 8 bytes in validatePacket
	if (pPayload[1] == 0 && pPayload[2] == 0 && pPayload[3] == 0) // Fragment offset is zero
		firstFramePart = true;

	if (!pGrouper->processPacket(pRTPPack, firstFramePart)) // TODO: error reporting?
		return;

	bool done = false;

	while (!done)
	{
		std::vector<uint8_t *> parts;
		std::vector<size_t> sizes;
		std::vector<real_t> receiveTimes;
		uint32_t timestamp;

		pGrouper->getNextQueuedPacket(parts, sizes, receiveTimes, timestamp);

		if (parts.size() == 0)
			done = true;
		else
		{
			processJPEGParts(parts, sizes, receiveTimes, timestamp, messages, timestamps);

			for (int i = 0 ; i < parts.size() ; i++)
				delete [] parts[i];
		}
	}
}

void MIPRTPJPEGDecoder::processJPEGParts(const std::vector<uint8_t *> &parts, const std::vector<size_t> &sizes, 
					 const std::vector<real_t> &receiveTimes, uint32_t timestamp,
 			                 std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps)

{
	int maxPacketSize = 0;

	// each size is at least 8 (validatePacket)
	for (int i = 0 ; i < parts.size() ; i++)
		maxPacketSize += sizes[i];

	maxPacketSize += 4096; // this should be more than enough to reconstruct the full JPEG frame

	uint8_t *pFirstPacket = parts[0];
	uint8_t typeSpecific = pFirstPacket[0];
	uint8_t headerType = pFirstPacket[4];
	uint8_t Q = pFirstPacket[5];
	int frameWidth = ((int)pFirstPacket[6]); //*8;
	int frameHeight = ((int)pFirstPacket[7]); //*8;

	// for the first packet, we expect at least a main JPEG header and a
	// quantization header

	if (!(headerType == 0 || headerType == 1)) // only these two are currently supported
		return;

	if (typeSpecific != 0) // only progressive scan is currently supported
		return;

	if (sizes[0] < 8+4)
		return;

	uint8_t mustBeZero = pFirstPacket[8];
	uint8_t precision = pFirstPacket[9];
	size_t quantLength = (size_t)(((int)pFirstPacket[10]) << 8)|((int)pFirstPacket[11]);

	if (quantLength + 8 + 4 > sizes[0]) // not enough bytes in the packet for the quantization table
		return;

	uint8_t stackLumaBuffer[64];
	uint8_t stackChromaBuffer[64];

	uint8_t *pLumaBuf = 0;
	uint8_t *pChromaBuf = 0;
	uint8_t *pFrameStart = 0;
	size_t frameBytesLeft = 0;

	if (Q < 128)
	{
		if (Q == 0 || Q > 99)
			return;

		pLumaBuf = stackLumaBuffer;
		pChromaBuf = stackChromaBuffer;

		MakeTables((int)Q, pLumaBuf, pChromaBuf);

		pFrameStart = pFirstPacket + 12;
		frameBytesLeft = sizes[0] - 12;
	}
	else // Q >= 128
	{
		// TODO: create a cache so that we can do something sensible if quantLength == 0,
		//       i.e. the table is not present in every single frame
		if (quantLength == 0)
		{
			std::cout << "quantLength == 0" << std::endl;
			return;
		}

		size_t lumaTableSize = 64;
		size_t chromeTableSize = 64;

		if (precision & 0x80) // high bit set
			lumaTableSize *= 2;
		if (precision & 0x40)
			chromeTableSize *= 2;

		if (quantLength != lumaTableSize + chromeTableSize)
			return;

		pLumaBuf = pFirstPacket + 12;
		pChromaBuf = pLumaBuf + lumaTableSize;

		pFrameStart = pChromaBuf + chromeTableSize;
		frameBytesLeft = sizes[0] - 12 - quantLength;
	}

	uint8_t *pFrameBuffer = new uint8_t[maxPacketSize];
	int bytesWritten = MakeHeaders(pFrameBuffer, headerType, frameWidth, frameHeight, pLumaBuf, pChromaBuf, 0); // no restarts

	if (frameBytesLeft > 0)
	{
		memcpy(pFrameBuffer + bytesWritten, pFrameStart, frameBytesLeft);
		bytesWritten += frameBytesLeft;
	}

	for (int i = 1 ; i < parts.size() ; i++)
	{
		if (sizes[i] < 8)
		{
			delete [] pFrameBuffer;
			return;
		}
		
		if (sizes[i] > 8)
		{
			memcpy(pFrameBuffer + bytesWritten, parts[i] + 8, sizes[i] - 8);
			bytesWritten += (sizes[i] - 8);
		}
	}

	// end of image marker
	pFrameBuffer[bytesWritten] = 0xff;
	pFrameBuffer[bytesWritten+1] = 0xd9; 
	bytesWritten += 2;

	real_t receiveTime = receiveTimes[0];

	// we'll take the minimal time as the time at which the packet was received
	for (int i = 1 ; i < receiveTimes.size() ; i++)
	{
		if (receiveTimes[i] < receiveTime)
			receiveTime = receiveTimes[i];
	}

	MIPEncodedVideoMessage *pVidMsg = new MIPEncodedVideoMessage(MIPENCODEDVIDEOMESSAGE_TYPE_JPEG, 0, 0, pFrameBuffer, bytesWritten, true);

	pVidMsg->setReceiveTime(receiveTime);

	messages.push_back(pVidMsg);
	timestamps.push_back(timestamp);
}

void MIPRTPJPEGDecoder::expireGroupers()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastCheckTime.getValue()) < 10.0)
		return;

	m_lastCheckTime = curTime;

	auto it = m_packetGroupers.begin(); 
	while (it != m_packetGroupers.end())
	{
		PacketGrouper *pPackGroup = (*it).second;

		if (pPackGroup->getLastAccessTime().getValue() - curTime.getValue() > 10.0) // TODO: make this configurable?
		{
			auto it2 = it;
			it++;

			delete (*it2).second;
			m_packetGroupers.erase(it2);
		}
		else
			it++;
	}
}
