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

#include "mipavcodecdecoder.h"
#include "mipencodedvideomessage.h"
#include "miprawvideomessage.h"

#include "mipdebug.h"

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

#define MIPAVCODECDECODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPAVCODECDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPAVCODECDECODER_ERRSTR_BADMESSAGE				"Bad message"
#define MIPAVCODECDECODER_ERRSTR_CANTINITAVCODEC			"Can't initialize avcodec library"
#define MIPAVCODECDECODER_ERRSTR_CANTCREATENEWDECODER			"Can't create new decoder"
#define MIPAVCODECDECODER_ERRSTR_CANTFINDCODEC				"Can't find codec"

MIPAVCodecDecoder::MIPAVCodecDecoder() : MIPComponent("MIPAVCodecDecoder")
{
	m_init = false;
}

MIPAVCodecDecoder::~MIPAVCodecDecoder()
{
	destroy();
}
	
bool MIPAVCodecDecoder::init(bool waitForKeyframe)
{
	if (m_init)
	{
		setErrorString(MIPAVCODECDECODER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_pCodec = avcodec_find_decoder(CODEC_ID_H263);
	if (m_pCodec == 0)
	{
		setErrorString(MIPAVCODECDECODER_ERRSTR_CANTFINDCODEC);
		return false;
	}

	m_pFrame = avcodec_alloc_frame();
#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	m_pFrameYUV420P = avcodec_alloc_frame();
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
	avcodec_get_frame_defaults(m_pFrame);
	
	m_lastIteration = -1;
	m_msgIt = m_messages.begin();
	m_lastExpireTime = MIPTime::getCurrentTime();
	m_init = true;
	m_waitForKeyframe = waitForKeyframe;
	return true;
}

bool MIPAVCodecDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPAVCODECDECODER_ERRSTR_NOTINIT);
		return false;
	}

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, DecoderInfo *>::iterator it;
#else
	hash_map<uint64_t, DecoderInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	for (it = m_decoderStates.begin() ; it != m_decoderStates.end() ; it++)
		delete (*it).second;
	m_decoderStates.clear();

	clearMessages();
	av_free(m_pFrame);
#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	av_free(m_pFrameYUV420P);
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
			
	m_init = false;

	return true;
}

bool MIPAVCodecDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAVCODECDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDVIDEOMESSAGE_TYPE_H263P))
	{
		setErrorString(MIPAVCODECDECODER_ERRSTR_BADMESSAGE);
		return false;
	}

	if (iteration != m_lastIteration)
	{
		m_lastIteration = iteration;
		clearMessages();
		expire();
	}

	MIPEncodedVideoMessage *pEncMsg = (MIPEncodedVideoMessage *)pMsg;

	uint64_t sourceID = pEncMsg->getSourceID();

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, DecoderInfo *>::iterator it;
#else
	hash_map<uint64_t, DecoderInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	DecoderInfo *pInf = 0;
	int width, height;

	width = pEncMsg->getWidth();
	height = pEncMsg->getHeight();

	it = m_decoderStates.find(sourceID);
	if (it == m_decoderStates.end()) // no entry found
	{
		AVCodecContext *pContext;

		pContext = avcodec_alloc_context();
		avcodec_get_context_defaults(pContext);
		pContext->width = 0; // let the codec work out the dimensions
		pContext->height = 0;
		if (avcodec_open(pContext, m_pCodec) < 0)
		{
			setErrorString(MIPAVCODECDECODER_ERRSTR_CANTCREATENEWDECODER);
			return false;
		}
		
#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
		pInf = new DecoderInfo(width, height, pContext);
#else
		// We'll allocate this later, when we know the dimensions of the video frame
		//SwsContext *pSwsContext = sws_getContext(width, height, pContext->pix_fmt, width, height, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

		//pInf = new DecoderInfo(width, height, pContext, pSwsContext);
		pInf = new DecoderInfo(width, height, pContext, 0);
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
		m_decoderStates[sourceID] = pInf;
	}
	else
	{
		pInf = (*it).second;
	
		if (!(pInf->getWidth() == width && pInf->getHeight() == height))
		{
			return true; // ignore message
		}

		pInf->setLastUpdateTime(MIPTime::getCurrentTime());
	}

	int framecomplete = 1;
	int status;

	uint8_t *pTmp = new uint8_t[pEncMsg->getDataLength() + FF_INPUT_BUFFER_PADDING_SIZE];
	memcpy(pTmp, pEncMsg->getImageData(), pEncMsg->getDataLength());
	memset(pTmp + pEncMsg->getDataLength(), 0, FF_INPUT_BUFFER_PADDING_SIZE);
	
#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	if ((status = avcodec_decode_video(pInf->getContext(), m_pFrame, &framecomplete, pTmp, (int)pEncMsg->getDataLength())) < 0)
	{	
		delete [] pTmp;
		return true; // unable to decode it, ignore
	}
#else
	AVPacket avPacket;

	av_init_packet(&avPacket);

	avPacket.size = (int)pEncMsg->getDataLength();
	avPacket.data = pTmp;

	if ((status = avcodec_decode_video2(pInf->getContext(), m_pFrame, &framecomplete, &avPacket)) < 0)
	{	
		delete [] pTmp;
		return true; // unable to decode it, ignore
	}
#endif

	delete [] pTmp;

	if (framecomplete)
	{
		bool skip = false;

		if (m_waitForKeyframe)
		{
			if (!pInf->getContext()->coded_frame->key_frame)
			{
				if (!pInf->receivedKeyframe())
					skip = true;
			}
			else
				pInf->setReceivedKeyframe(true);
		}

		if (!skip)
		{
			// adjust width and height settings

			width = pInf->getContext()->width;
			height = pInf->getContext()->height;

			size_t dataSize = (width*height*3)/2;
			uint8_t *pData = new uint8_t [dataSize];

#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
			avpicture_fill((AVPicture *)m_pFrameYUV420P, pData, PIX_FMT_YUV420P, width, height);
			img_convert((AVPicture *)m_pFrameYUV420P, PIX_FMT_YUV420P, (AVPicture*)m_pFrame, pInf->getContext()->pix_fmt, width, height);
#else
			SwsContext *pSwsContext = pInf->getSwsContext();

			// TODO: check if width and height changed somehow?
			if (pSwsContext == 0)
			{
				pSwsContext = sws_getContext(width, height, pInf->getContext()->pix_fmt, width, height, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

				pInf->setSwsContext(pSwsContext);
			}

			uint8_t *pDstPointers[3];
			int dstStrides[3];
		
			pDstPointers[0] = pData;
			pDstPointers[1] = pData+width*height;
			pDstPointers[2] = pDstPointers[1]+(width*height)/4;
			dstStrides[0] = width;
			dstStrides[1] = width/2;
			dstStrides[2] = width/2;

			sws_scale(pSwsContext, m_pFrame->data, m_pFrame->linesize, 0, height, pDstPointers, dstStrides);
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
		
			MIPRawYUV420PVideoMessage *pNewMsg = new MIPRawYUV420PVideoMessage(width, height, pData, true);

			pNewMsg->setSourceID(sourceID);
			pNewMsg->setTime(pEncMsg->getTime());

			m_messages.push_back(pNewMsg);
			m_msgIt = m_messages.begin();
		}
	}
	
	return true;
}

bool MIPAVCodecDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAVCODECDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_lastIteration)
	{
		m_lastIteration = iteration;
		clearMessages();
		expire();
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

void MIPAVCodecDecoder::clearMessages()
{
	std::list<MIPRawYUV420PVideoMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPAVCodecDecoder::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastExpireTime.getValue()) < 60.0)
		return;

#if defined(WIN32) || defined(_WIN32_WCE)
	hash_map<uint64_t, DecoderInfo *>::iterator it;
#else
	hash_map<uint64_t, DecoderInfo *, __gnu_cxx::hash<uint32_t> >::iterator it;
#endif // Win32

	it = m_decoderStates.begin();
	while (it != m_decoderStates.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
#if defined(WIN32) || defined(_WIN32_WCE)
			hash_map<uint64_t, DecoderInfo *>::iterator it2 = it;
#else
			hash_map<uint64_t, DecoderInfo *, __gnu_cxx::hash<uint32_t> >::iterator it2 = it;
#endif // Win32
			it++;
	
			delete (*it2).second;
			m_decoderStates.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
}

void MIPAVCodecDecoder::initAVCodec()
{
	avcodec_init();
	avcodec_register_all();
	av_log_set_level(AV_LOG_QUIET);
}


#endif // MIPCONFIG_SUPPORT_AVCODEC

