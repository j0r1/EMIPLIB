/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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
#include <iostream>

#include "mipdebug.h"

#define MIPAVCODECENCODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPAVCODECENCODER_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPAVCODECENCODER_ERRSTR_BADFRAMERATE					"Bad framerate"
#define MIPAVCODECENCODER_ERRSTR_CANTFINDCODEC					"Can't find codec"
#define MIPAVCODECENCODER_ERRSTR_CANTCREATECONTEXT				"Can't create context"
#define MIPAVCODECENCODER_ERRSTR_CANTINITCONTEXT				"Can't init context"
#define MIPAVCODECENCODER_ERRSTR_BADMESSAGE					"Bad message"
#define MIPAVCODECENCODER_ERRSTR_BADDIMENSIONS					"Invalid image width or height"
#define MIPAVCODECENCODER_ERRSTR_CANTENCODE					"Error encoding frame"
#define MIPAVCODECENCODER_ERRSTR_CANTFILLPICTURE				"Can't fill picture"

MIPAVCodecEncoder::MIPAVCodecEncoder() : MIPComponent("MIPAVCodecEncoder")
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
		
	m_pCodec = avcodec_find_encoder(CODEC_ID_H263P);
	if (m_pCodec == 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTFINDCODEC);
		return false;
	}
	
	m_pContext = avcodec_alloc_context();
	
	avcodec_get_context_defaults(m_pContext);
	m_pContext->width = width;
	m_pContext->height = height;
	m_pContext->pix_fmt = PIX_FMT_YUV420P;
#if LIBAVCODEC_VERSION_INT > 0x0409
	m_pContext->time_base.num = denominator; // time_base = 1/framerate
	m_pContext->time_base.den = numerator;
#else
	m_pContext->frame_rate = numerator;
	m_pContext->frame_rate_base = denominator;
#endif // LIBAVCODEC_VERSION_INT > 0x0409
	if (bitrate > 0)
	{
		m_pContext->bit_rate = bitrate;
		m_pContext->bit_rate_tolerance = bitrate/20; // 5%
	}
	
	if (avcodec_open(m_pContext, m_pCodec) < 0)
	{
		m_pCodec = 0;
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTINITCONTEXT);
		return false;
	}

	m_pFrame = avcodec_alloc_frame();
	avcodec_get_frame_defaults(m_pFrame);
	
	m_width = width;
	m_height = height;
	m_bufSize = (width*height*3)/2;
	m_pData = new uint8_t [m_bufSize];
	m_pMsg = new MIPEncodedVideoMessage(MIPENCODEDVIDEOMESSAGE_TYPE_H263P, width, height, m_pData, 0, true);
	m_gotMessage = false;
	
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
	av_free(m_pFrame);
	m_pCodec = 0;
	
	if (m_pMsg)
	{
		delete m_pMsg;
		m_pMsg = 0;
	}

	return true;
}

bool MIPAVCodecEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
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

	if (avpicture_fill((AVPicture *)m_pFrame, (uint8_t *)pVideoMsg->getImageData(), PIX_FMT_YUV420P, m_width, m_height) < 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTFILLPICTURE);
		return false;
	}
	
	int status;
	
	if ((status = avcodec_encode_video(m_pContext, m_pData, m_bufSize, m_pFrame)) < 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_CANTENCODE);
		return false;
	}
	
	m_pMsg->setDataLength(status);
	m_pMsg->copyMediaInfoFrom(*pVideoMsg);
	
	m_gotMessage = false;

	return true;
}

bool MIPAVCodecEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pCodec == 0)
	{
		setErrorString(MIPAVCODECENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (m_gotMessage)
	{
		*pMsg = 0;
		m_gotMessage = false;
	}
	else
	{
		*pMsg = m_pMsg;
		m_gotMessage = true;
	}
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
	avcodec_init();
	avcodec_register_all();
	av_log_set_level(AV_LOG_QUIET);
}

#endif // MIPCONFIG_SUPPORT_AVCODEC

