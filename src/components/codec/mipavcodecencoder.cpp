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

#include "mipavcodecencoder.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"

extern "C" {
#include <libavutil/imgutils.h>
}

#include "mipdebug.h"

#define MIPAVCODECENCODER_ERRSTR_NOTINIT						"Not initialized"
#define MIPAVCODECENCODER_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPAVCODECENCODER_ERRSTR_BADFRAMERATE					"Bad framerate"
#define MIPAVCODECENCODER_ERRSTR_CANTFINDCODEC					"Can't find codec"
#define MIPAVCODECENCODER_ERRSTR_CANTCREATECONTEXT				"Can't create context"
#define MIPAVCODECENCODER_ERRSTR_CANTINITCONTEXT				"Can't init context"
#define MIPAVCODECENCODER_ERRSTR_BADMESSAGE						"Bad message"
#define MIPAVCODECENCODER_ERRSTR_BADDIMENSIONS					"Invalid image width or height"
#define MIPAVCODECENCODER_ERRSTR_CANTENCODE						"Error encoding frame"
#define MIPAVCODECENCODER_ERRSTR_CANTFILLPICTURE				"Can't fill picture"

MIPAVCodecEncoder::MIPAVCodecEncoder() : MIPOutputMessageQueue("MIPAVCodecEncoder")
{
	m_pCodec = 0;
}

MIPAVCodecEncoder::~MIPAVCodecEncoder()
{
	destroy();
}

bool MIPAVCodecEncoder::init(int width, int height, real_t framerate, int bitrate)
{
	if (m_pCodec != 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_ALREADYINIT);
		return false;
	}
	
	int numerator,denominator;
	
	if (!getFrameRate(framerate, &numerator, &denominator))
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_BADFRAMERATE);
		return false;
	}
		
	m_pCodec = avcodec_find_encoder(AV_CODEC_ID_H263P);
	if (m_pCodec == 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTFINDCODEC);
		return false;
	}
	
	m_pContext = avcodec_alloc_context3(m_pCodec);
	
	m_pContext->width = width;
	m_pContext->height = height;
	m_pContext->pix_fmt = AV_PIX_FMT_YUV420P;
	m_pContext->time_base.num = denominator; // time_base = 1/framerate
	m_pContext->time_base.den = numerator;
	if (bitrate > 0)
	{
		m_pContext->bit_rate = bitrate;
		m_pContext->bit_rate_tolerance = bitrate/20; // 5%
	}
	
	if (avcodec_open2(m_pContext, m_pCodec, nullptr) < 0)
	{
		m_pCodec = 0;
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTINITCONTEXT);
		return false;
	}

	m_pFrame = av_frame_alloc();
	m_pFrame->format = m_pContext->pix_fmt;
	m_pFrame->width = m_pContext->width;
	m_pFrame->height = m_pContext->height;
	
	m_width = width;
	m_height = height;
	
	MIPOutputMessageQueue::init();

	return true;
}

bool MIPAVCodecEncoder::destroy()
{
	if (m_pCodec == 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_NOTINIT);
		return false;
	}

	avcodec_close(m_pContext);
	av_free(m_pContext);
	av_frame_free(&m_pFrame);
	m_pCodec = 0;
	
	MIPOutputMessageQueue::clearMessages();

	return true;
}

bool MIPAVCodecEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	MIPOutputMessageQueue::checkIteration(iteration);
	
	if (m_pCodec == 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUV420P))
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRawYUV420PVideoMessage *pVideoMsg = (MIPRawYUV420PVideoMessage *)pMsg;

	if (!(pVideoMsg->getWidth() == m_width && pVideoMsg->getHeight() == m_height))
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_BADDIMENSIONS);
		return false;
	}

	for (int i = 0 ; i < AV_NUM_DATA_POINTERS ; i++)
	{
		m_pFrame->data[i] = nullptr;
		m_pFrame->linesize[i] = 0;
	}

	if (av_image_fill_arrays(m_pFrame->data, m_pFrame->linesize, (uint8_t *)pVideoMsg->getImageData(),
	                         AV_PIX_FMT_YUV420P, m_width, m_height, 1) < 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTFILLPICTURE);
		return false;
	}
	
	int status;

	if ((status = avcodec_send_frame(m_pContext, m_pFrame)) < 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTENCODE);
		return false;
	}

	AVPacket pkt;

	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;

	if ((status = avcodec_receive_packet(m_pContext, &pkt)) < 0)
	{
		av_packet_unref(&pkt);
		if (status == AVERROR(EAGAIN)) // not really an error, ignore and try again later
			return true;

		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTENCODE);
		return false;
	}
	
	uint8_t *pData = new uint8_t [pkt.size];
	MIPEncodedVideoMessage *pNewMsg = new MIPEncodedVideoMessage(MIPENCODEDVIDEOMESSAGE_TYPE_H263P, m_width, m_height, pData, pkt.size, true);
	memcpy(pData, pkt.data, pkt.size);
	av_packet_unref(&pkt);

	pNewMsg->copyMediaInfoFrom(*pVideoMsg);
	
	MIPOutputMessageQueue::addToOutputQueue(pNewMsg, true);

	return true;
}

bool MIPAVCodecEncoder::getFrameRate(real_t framerate, int *numerator, int *denominator)
{
	real_t rate = framerate;
	int scale = 1;
	
	while (scale < 10000000)
	{
		real_t diff = (rate - (real_t)((int)(rate+0.5)));

		if (diff < 0.00000001 && diff > -0.00000001)
		{
			*numerator = (int)rate;
			*denominator = scale;
			return true;
		}
		else
		{
			scale *= 10;
			rate *= 10.0;
		}
	}
	return false;
}

void MIPAVCodecEncoder::initAVCodec()
{
	avcodec_register_all();
	av_log_set_level(AV_LOG_QUIET);
}

#endif // MIPCONFIG_SUPPORT_AVCODEC

