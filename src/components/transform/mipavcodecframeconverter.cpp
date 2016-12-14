/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

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
		m_targetPixFmt = PIX_FMT_YUV420P;
		break;
	case MIPRAWVIDEOMESSAGE_TYPE_YUYV:
#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_UNSUPPORTEDSUBTYPE);
#else
		m_targetPixFmt = PIX_FMT_YUYV422;
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
		break;
	default:
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_INVALIDSUBTYPE);
		return false;
	}
	
	m_targetSubtype = targetSubtype;

	if (targetWidth < 2 || targetHeight < 2 || targetWidth > 10000 || targetHeight > 10000)
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_INVALIDDIMENSION);
		return false;
	}

	m_targetWidth = targetWidth;
	m_targetHeight = targetHeight;

#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	m_pTargetAVFrame = avcodec_alloc_frame();
	m_pSrcAVFrame = avcodec_alloc_frame();
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD

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

#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	if (m_pTargetAVFrame)
		av_free(m_pTargetAVFrame);
	if (m_pSrcAVFrame)
		av_free(m_pSrcAVFrame);
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD

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
	if (!(subType == MIPRAWVIDEOMESSAGE_TYPE_YUYV || subType == MIPRAWVIDEOMESSAGE_TYPE_YUV420P))
	{
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPVideoMessage *pVideoMsg = (MIPVideoMessage *)pMsg;
	uint64_t sourceID = pVideoMsg->getSourceID();
	int width = pVideoMsg->getWidth();
	int height = pVideoMsg->getHeight();
	MIPVideoMessage *pNewMsg = 0;

#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	PixelFormat srcPixFmt;

	if (subType == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
	{
		MIPRawYUV420PVideoMessage *pRawMsg = (MIPRawYUV420PVideoMessage *)pVideoMsg;

		srcPixFmt = PIX_FMT_YUV420P;
		avpicture_fill((AVPicture *)m_pSrcAVFrame, (uint8_t *)pRawMsg->getImageData(), PIX_FMT_YUV420P, width, height);
	}
	else // MIPRAWVIDEOMESSAGE_TYPE_YUYV
	{
	/*
		MIPRawYUYVVideoMessage *pRawMsg = (MIPRawYUYVVideoMessage *)pVideoMsg;

		srcPixFmt = PIX_FMT_YUYV422;
		avpicture_fill((AVPicture *)m_pSrcAVFrame, pRawMsg->getImageData(), PIX_FMT_YUYV422, width, height);
	*/
		setErrorString(MIPAVCODECFRAMECONVERTER_ERRSTR_BADMESSAGE);
		return false;
	}

	uint8_t *pData = 0;

	if (m_targetSubtype == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
	{
		pData = new uint8_t[(m_targetWidth*m_targetHeight*3)/2];
		pNewMsg = new MIPRawYUV420PVideoMessage(m_targetWidth, m_targetHeight, pData, true);
	}
	else // MIPRAWVIDEOMESSAGE_TYPE_YUYV
	{
		/* Shouldn't get here */
		/*
		pData = new uint8_t[m_targetWidth*m_targetHeight*2];
		pNewMsg = new MIPRawYUYVVideoMessage(m_targetWidth, m_targetHeight, pData, true);
		*/
	}
	avpicture_fill((AVPicture *)m_pTargetAVFrame, pData, m_targetPixFmt, m_targetWidth, m_targetHeight);

	img_convert((AVPicture *)m_pTargetAVFrame, srcPixFmt, (AVPicture *)m_pSrcAVFrame, m_targetPixFmt, m_targetWidth, m_targetHeight);

#else // swscale version

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, ConvertCache *>::iterator it;
#else
	hash_map<uint64_t, ConvertCache *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	ConvertCache *pCache = 0;

	it = m_convertCache.find(sourceID);
	if (it == m_convertCache.end()) // no entry found
	{
		PixelFormat srcPixFmt;

		if (subType == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
			srcPixFmt = PIX_FMT_YUV420P;
		else
			srcPixFmt = PIX_FMT_YUYV422;

		SwsContext *pSwsContext = sws_getContext(width, height, srcPixFmt, m_targetWidth, m_targetHeight, m_targetPixFmt, SWS_FAST_BILINEAR, 0, 0, 0);

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

	uint8_t *pSrcPointers[3];
	int srcStrides[3];

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
		uint8_t *pData = new uint8_t[(m_targetWidth*m_targetHeight*3)/2];
		uint8_t *pDstPointers[3];
		int dstStrides[3];
	
		pDstPointers[0] = pData;
		pDstPointers[1] = pData+m_targetWidth*m_targetHeight;
		pDstPointers[2] = pDstPointers[1]+(m_targetWidth*m_targetHeight)/4;
		dstStrides[0] = m_targetWidth;
		dstStrides[1] = m_targetWidth/2;
		dstStrides[2] = m_targetWidth/2;

		sws_scale(pCache->getSwsContext(), pSrcPointers, srcStrides, 0, height, pDstPointers, dstStrides);

		pNewMsg = new MIPRawYUV420PVideoMessage(m_targetWidth, m_targetHeight, pData, true);
	}
	else // MIPRAWVIDEOMESSAGE_TYPE_YUYV
	{
		uint8_t *pData = new uint8_t[m_targetWidth*m_targetHeight*2];
		uint8_t *pDstPointers[1];
		int dstStrides[1];
	
		pDstPointers[0] = pData;
		dstStrides[0] = m_targetWidth*2;

		sws_scale(pCache->getSwsContext(), pSrcPointers, srcStrides, 0, height, pDstPointers, dstStrides);

		pNewMsg = new MIPRawYUYVVideoMessage(m_targetWidth, m_targetHeight, pData, true);
	}

#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD

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
#ifndef MIPCONFIG_SUPPORT_AVCODEC_OLD
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastExpireTime.getValue()) < 60.0)
		return;

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, ConvertCache *>::iterator it;
#else
	hash_map<uint64_t, ConvertCache *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_convertCache.begin();
	while (it != m_convertCache.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
#if defined(WIN32) || defined(_WIN32_WCE)
			hash_map<uint64_t, ConvertCache *>::iterator it2 = it;
#else
			hash_map<uint64_t, ConvertCache *, __gnu_cxx::hash<uint32_t> >::iterator it2 = it;
#endif // Win32
			it++;
	
			delete (*it2).second;
			m_convertCache.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
#endif // ! MIPCONFIG_SUPPORT_AVCODEC_OLD
}

void MIPAVCodecFrameConverter::clearCache()
{
#ifndef MIPCONFIG_SUPPORT_AVCODEC_OLD
#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, ConvertCache *>::iterator it;
#else
	hash_map<uint64_t, ConvertCache *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	for (it = m_convertCache.begin() ; it != m_convertCache.end() ; it++)
		delete (*it).second;
	m_convertCache.clear();
#endif // ! MIPCONFIG_SUPPORT_AVCODEC_OLD
}

void MIPAVCodecFrameConverter::initAVCodec()
{
	avcodec_init();
	avcodec_register_all();
	av_log_set_level(AV_LOG_QUIET);
}

#endif // MIPCONFIG_SUPPORT_AVCODEC
