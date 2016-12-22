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

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_AVCODEC

#include "mipavcodecframeconverter.h"
#include "miprawvideomessage.h"

#include "mipdebug.h"

#define MIPAVCODECFRAMECONVERTER_ERRSTR_ALREADYINITIALIZED		"Already initialized"
#define MIPAVCODECFRAMECONVERTER_ERRSTR_NOTINITIALIZED			"Not initialized"
#define MIPAVCODECFRAMECONVERTER_ERRSTR_INVALIDDIMENSION		"Invalid target width or height"
#define MIPAVCODECFRAMECONVERTER_ERRSTR_INVALIDSUBTYPE			"Invalid target subtype"
#define MIPAVCODECFRAMECONVERTER_ERRSTR_UNSUPPORTEDSUBTYPE		"Unsupported target subtype"
#define MIPAVCODECFRAMECONVERTER_ERRSTR_BADMESSAGE			"Invalid message type or subtype"

MIPAVCodecFrameConverter::MIPAVCodecFrameConverter() : MIPComponent("MIPAVCodecFrameConverter")
{
	m_init = false;
}

MIPAVCodecFrameConverter::~MIPAVCodecFrameConverter()
{
	destroy();
}

bool MIPAVCodecFrameConverter::init(int targetWidth, int targetHeight, uint32_t targetSubtype)
{
	if (m_init)
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_ALREADYINITIALIZED);
		return false;
	}

	switch(targetSubtype)
	{
	case MIPRAWVIDEOMESSAGE_TYPE_YUV420P:
		m_targetPixFmt = AV_PIX_FMT_YUV420P;
		break;
	case MIPRAWVIDEOMESSAGE_TYPE_YUYV:
		m_targetPixFmt = AV_PIX_FMT_YUYV422;
		break;
	case MIPRAWVIDEOMESSAGE_TYPE_RGB24:
		m_targetPixFmt = AV_PIX_FMT_RGB24;
		break;
	case MIPRAWVIDEOMESSAGE_TYPE_RGB32:
		m_targetPixFmt = AV_PIX_FMT_RGBA;
		break;
	default:
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_INVALIDSUBTYPE);
		return false;
	}
	
	m_targetSubtype = targetSubtype;

	if (targetWidth < 0 || targetHeight < 0)
	{
		m_targetWidth = -1;
		m_targetHeight = -1;
	}
	else
	{
		if (targetWidth < 2 || targetHeight < 2 || targetWidth > 10000 || targetHeight > 10000)
		{
			setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_INVALIDDIMENSION);
			return false;
		}

		m_targetWidth = targetWidth;
		m_targetHeight = targetHeight;
	}

	m_lastIteration = -1;
	m_init = true;
	m_msgIt = m_messages.begin();
	return true;
}

bool MIPAVCodecFrameConverter::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	clearMessages();
	clearCache();

	m_init = false;
	
	return true;
}

bool MIPAVCodecFrameConverter::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_NOTINITIALIZED);
		return false;
	}
	
	if (m_lastIteration != iteration)
	{
		clearMessages();
		expire();
		m_lastIteration = iteration;
	}

	if (pMsg->getMessageType() != MIPMESSAGE_TYPE_VIDEO_RAW)
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_BADMESSAGE);
		return false;
	}

	uint32_t subType = pMsg->getMessageSubtype();
	if (!(subType == MIPRAWVIDEOMESSAGE_TYPE_YUYV || subType == MIPRAWVIDEOMESSAGE_TYPE_YUV420P || 
	      subType == MIPRAWVIDEOMESSAGE_TYPE_RGB24 || subType == MIPRAWVIDEOMESSAGE_TYPE_RGB32))
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPVideoMessage *pVideoMsg = (MIPVideoMessage *)pMsg;
	uint64_t sourceID = pVideoMsg->getSourceID();
	int width = pVideoMsg->getWidth();
	int height = pVideoMsg->getHeight();
	MIPVideoMessage *pNewMsg = 0;
	int targetWidth = m_targetWidth;
	int targetHeight = m_targetHeight;

	if (targetWidth == -1)
	{
		targetWidth = width;
		targetHeight = height;
	}

	ConvertCache *pCache = 0;

	auto it = m_convertCache.find(sourceID);
	if (it == m_convertCache.end()) // no entry found
	{
		AVPixelFormat srcPixFmt;

		if (subType == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
			srcPixFmt = AV_PIX_FMT_YUV420P;
		else if (subType == MIPRAWVIDEOMESSAGE_TYPE_RGB24)
			srcPixFmt = AV_PIX_FMT_RGB24;
		else if (subType == MIPRAWVIDEOMESSAGE_TYPE_RGB32)
			srcPixFmt = AV_PIX_FMT_RGBA;
		else
			srcPixFmt = AV_PIX_FMT_YUYV422;

		SwsContext *pSwsContext = sws_getContext(width, height, srcPixFmt, targetWidth, targetHeight, m_targetPixFmt, SWS_FAST_BILINEAR, 0, 0, 0);

		pCache = new ConvertCache(width, height, subType, pSwsContext);

		m_convertCache[sourceID] = pCache;
	}
	else
		pCache = (*it).second;

	if (!(pCache->getSourceWidth() == width && pCache->getSourceHeight() == height && pCache->getSourceSubtype() == subType))
	{
		// ignore message
		return true;
	}

	uint8_t *pSrcPointers[4];
	int srcStrides[4];

	if (subType == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
	{
		MIPRawYUV420PVideoMessage *pRawMsg = (MIPRawYUV420PVideoMessage *)pVideoMsg;

		pSrcPointers[0] = (uint8_t *)pRawMsg->getImageData();
		pSrcPointers[1] = pSrcPointers[0] + width*height;
		pSrcPointers[2] = pSrcPointers[1] + (width*height)/4;
		srcStrides[0] = width;
		srcStrides[1] = width/2;
		srcStrides[2] = width/2;
	}
	else if (subType == MIPRAWVIDEOMESSAGE_TYPE_RGB24)
	{
		MIPRawRGBVideoMessage *pRawMsg = (MIPRawRGBVideoMessage *)pVideoMsg;

		pSrcPointers[0] = (uint8_t *)pRawMsg->getImageData();
		pSrcPointers[1] = pSrcPointers[0]+1;
		pSrcPointers[2] = pSrcPointers[0]+2;
		srcStrides[0] = width*3;
		srcStrides[1] = width*3;
		srcStrides[2] = width*3;
	}
	else if (subType == MIPRAWVIDEOMESSAGE_TYPE_RGB32)
	{
		MIPRawRGBVideoMessage *pRawMsg = (MIPRawRGBVideoMessage *)pVideoMsg;

		pSrcPointers[0] = (uint8_t *)pRawMsg->getImageData();
		pSrcPointers[1] = pSrcPointers[0]+1;
		pSrcPointers[2] = pSrcPointers[0]+2;
		pSrcPointers[3] = pSrcPointers[0]+3;
		srcStrides[0] = width*4;
		srcStrides[1] = width*4;
		srcStrides[2] = width*4;
		srcStrides[3] = width*4;
	}
	else // MIPRAWVIDEOMESSAGE_TYPE_YUYV
	{
		MIPRawYUYVVideoMessage *pRawMsg = (MIPRawYUYVVideoMessage *)pVideoMsg;

		pSrcPointers[0] = (uint8_t *)pRawMsg->getImageData();
		pSrcPointers[1] = pSrcPointers[0]+1;
		pSrcPointers[2] = pSrcPointers[1]+2;
		srcStrides[0] = width*2;
		srcStrides[1] = width*2;
		srcStrides[2] = width*2;
	}

	if (m_targetSubtype == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
	{
		uint8_t *pData = new uint8_t[(targetWidth*targetHeight*3)/2];
		uint8_t *pDstPointers[3];
		int dstStrides[3];
	
		pDstPointers[0] = pData;
		pDstPointers[1] = pData+targetWidth*targetHeight;
		pDstPointers[2] = pDstPointers[1]+(targetWidth*targetHeight)/4;
		dstStrides[0] = targetWidth;
		dstStrides[1] = targetWidth/2;
		dstStrides[2] = targetWidth/2;

		sws_scale(pCache->getSwsContext(), pSrcPointers, srcStrides, 0, height, pDstPointers, dstStrides);

		pNewMsg = new MIPRawYUV420PVideoMessage(targetWidth, targetHeight, pData, true);
	}
	else if (m_targetSubtype == MIPRAWVIDEOMESSAGE_TYPE_RGB24)
	{
		uint8_t *pData = new uint8_t[targetWidth*targetHeight*3];
		uint8_t *pDstPointers[3];
		int dstStrides[3];
	
		pDstPointers[0] = pData;
		pDstPointers[1] = pData+1;
		pDstPointers[2] = pData+2;
		dstStrides[0] = targetWidth*3;
		dstStrides[1] = targetWidth*3;
		dstStrides[2] = targetWidth*3;

		sws_scale(pCache->getSwsContext(), pSrcPointers, srcStrides, 0, height, pDstPointers, dstStrides);

		pNewMsg = new MIPRawRGBVideoMessage(targetWidth, targetHeight, pData, false, true);
	}
	else if (m_targetSubtype == MIPRAWVIDEOMESSAGE_TYPE_RGB32)
	{
		uint8_t *pData = new uint8_t[targetWidth*targetHeight*4];
		uint8_t *pDstPointers[4];
		int dstStrides[4];
	
		pDstPointers[0] = pData;
		pDstPointers[1] = pData+1;
		pDstPointers[2] = pData+2;
		pDstPointers[3] = pData+3;
		dstStrides[0] = targetWidth*4;
		dstStrides[1] = targetWidth*4;
		dstStrides[2] = targetWidth*4;
		dstStrides[3] = targetWidth*4;

		sws_scale(pCache->getSwsContext(), pSrcPointers, srcStrides, 0, height, pDstPointers, dstStrides);

		pNewMsg = new MIPRawRGBVideoMessage(targetWidth, targetHeight, pData, true, true);
	}
	else // MIPRAWVIDEOMESSAGE_TYPE_YUYV
	{
		uint8_t *pData = new uint8_t[targetWidth*targetHeight*2];
		uint8_t *pDstPointers[1];
		int dstStrides[1];
	
		pDstPointers[0] = pData;
		dstStrides[0] = targetWidth*2;

		sws_scale(pCache->getSwsContext(), pSrcPointers, srcStrides, 0, height, pDstPointers, dstStrides);

		pNewMsg = new MIPRawYUYVVideoMessage(targetWidth, targetHeight, pData, true);
	}

	pNewMsg->copyMediaInfoFrom(*pVideoMsg);

	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();

	return true;
}

bool MIPAVCodecFrameConverter::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	if (m_lastIteration != iteration)
	{
		clearMessages();
		expire();
		m_lastIteration = iteration;
	}

	if (m_msgIt == m_messages.end())
	{
		*pMsg = 0;
		m_msgIt = m_messages.begin();
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	return true;
}

void MIPAVCodecFrameConverter::clearMessages()
{
	std::list<MIPVideoMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPAVCodecFrameConverter::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastExpireTime.getValue()) < 60.0)
		return;

	auto it = m_convertCache.begin();
	while (it != m_convertCache.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
			auto it2 = it;
			it++;
	
			delete (*it2).second;
			m_convertCache.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
}

void MIPAVCodecFrameConverter::clearCache()
{
	for (auto it = m_convertCache.begin() ; it != m_convertCache.end() ; it++)
		delete (*it).second;
	m_convertCache.clear();
}

void MIPAVCodecFrameConverter::initAVCodec()
{
	avcodec_register_all();
	av_log_set_level(AV_LOG_QUIET);
}

#endif // MIPCONFIG_SUPPORT_AVCODEC

