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
#include <vector>

#include "mipdebug.h"

using namespace std;

#define MIPAVCODECDECODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPAVCODECDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPAVCODECDECODER_ERRSTR_BADMESSAGE					"Bad message"
#define MIPAVCODECDECODER_ERRSTR_CANTINITAVCODEC			"Can't initialize avcodec library"
#define MIPAVCODECDECODER_ERRSTR_CANTCREATENEWDECODER		"Can't create new decoder"
#define MIPAVCODECDECODER_ERRSTR_CANTFINDCODEC				"Can't find codec"
#define MIPAVCODECDECODER_ERRSTR_CANTCREATECONTEXT			"Can't create context for codec"

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

	m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H263);
	if (m_pCodec == 0)
	{
		setErrorString(MIPAVCODECDECODER_ERRSTR_CANTFINDCODEC);
		return false;
	}

	m_pFrame = av_frame_alloc();
	
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

	for (auto it = m_decoderStates.begin() ; it != m_decoderStates.end() ; it++)
		delete it->second;
	m_decoderStates.clear();

	clearMessages();
	av_frame_free(&m_pFrame);
			
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

	DecoderInfo *pInf = 0;
	int width, height;

	width = pEncMsg->getWidth();
	height = pEncMsg->getHeight();

	auto it = m_decoderStates.find(sourceID);
	if (it == m_decoderStates.end()) // no entry found
	{
		AVCodecContext *pContext;

		pContext = avcodec_alloc_context3(m_pCodec);
		if (!pContext)
		{
			setErrorString(MIPAVCODECDECODER_ERRSTR_CANTCREATECONTEXT);
			return false;
		}
		
		pContext->width = 0; // let the codec work out the dimensions
		pContext->height = 0;

		if (avcodec_open2(pContext, m_pCodec, nullptr) < 0)
		{
			av_free(pContext);
			setErrorString(MIPAVCODECDECODER_ERRSTR_CANTCREATENEWDECODER);
			return false;
		}
		
		// We'll allocate this later, when we know the dimensions of the video frame
		//SwsContext *pSwsContext = sws_getContext(width, height, pContext->pix_fmt, width, height, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
		//pInf = new DecoderInfo(width, height, pContext, pSwsContext);

		pInf = new DecoderInfo(width, height, pContext, 0);
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

	int status;

	vector<uint8_t> tmp(pEncMsg->getDataLength() + AV_INPUT_BUFFER_PADDING_SIZE);
	uint8_t *pTmp = &tmp[0];

	memcpy(pTmp, pEncMsg->getImageData(), pEncMsg->getDataLength());
	memset(pTmp + pEncMsg->getDataLength(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
	
	AVPacket avPacket;

	av_init_packet(&avPacket);

	avPacket.size = (int)pEncMsg->getDataLength();
	avPacket.data = pTmp;

	if ((status = avcodec_send_packet(pInf->getContext(), &avPacket)) < 0)
	{
		// unable to decode it, ignore
		return true; 
	}

	if ((status = avcodec_receive_frame(pInf->getContext(), m_pFrame)) < 0)
	{
		// unable to decode it, ignore
		return true;
	}

	bool skip = false;

#if 0 // TODO: find something for this?
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
#endif

	if (!skip)
	{
		// adjust width and height settings

		width = pInf->getContext()->width;
		height = pInf->getContext()->height;

		size_t dataSize = (width*height*3)/2;
		uint8_t *pData = new uint8_t [dataSize];

		SwsContext *pSwsContext = pInf->getSwsContext();

		// TODO: check if width and height changed somehow?
		if (pSwsContext == 0)
		{
			pSwsContext = sws_getContext(width, height, pInf->getContext()->pix_fmt, width, height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

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
	
		MIPRawYUV420PVideoMessage *pNewMsg = new MIPRawYUV420PVideoMessage(width, height, pData, true);

		pNewMsg->setSourceID(sourceID);
		pNewMsg->setTime(pEncMsg->getTime());

		m_messages.push_back(pNewMsg);
		m_msgIt = m_messages.begin();
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

	auto it = m_decoderStates.begin();
	while (it != m_decoderStates.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
			auto it2 = it;
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
	avcodec_register_all();
	av_log_set_level(AV_LOG_QUIET);
}


#endif // MIPCONFIG_SUPPORT_AVCODEC

