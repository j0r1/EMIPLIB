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
#include "miptinyjpegdecoder.h"
#include "mipencodedvideomessage.h"
#include "miprawvideomessage.h"
#include "tinyjpeg.h"

#include "mipdebug.h"

#define MIPTINYJPEGDECODER_ERRSTR_NOTINIT			"Not initialized"
#define MIPTINYJPEGDECODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPTINYJPEGDECODER_ERRSTR_BADMESSAGE			"Only JPEG compressed frames are accepted"
#define MIPTINYJPEGDECODER_ERRSTR_CANTINITDECODER		"Can't initialize the Tiny Jpeg Decoder"

MIPTinyJPEGDecoder::MIPTinyJPEGDecoder() : MIPComponent("MIPTinyJPEGDecoder")
{
	m_init = false;
}

MIPTinyJPEGDecoder::~MIPTinyJPEGDecoder()
{
	destroy();
}

bool MIPTinyJPEGDecoder::init()
{
	if (m_init)
	{
		setErrorString(MIPTINYJPEGDECODER_ERRSTR_ALREADYINIT);
		return false;
	}
	
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_init = true;
	
	return true;
}

bool MIPTinyJPEGDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPTINYJPEGDECODER_ERRSTR_NOTINIT);
		return false;
	}
	
	clearMessages();
	m_init = false;

	return true;
}

bool MIPTinyJPEGDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPTINYJPEGDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDVIDEOMESSAGE_TYPE_JPEG) ) 
	{
		setErrorString(MIPTINYJPEGDECODER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPEncodedVideoMessage *pEncMsg = (MIPEncodedVideoMessage *)pMsg;
	int status;
	struct jdec_private *pDec;

	pDec = tinyjpeg_init();
	if (pDec == 0)
	{
		setErrorString(MIPTINYJPEGDECODER_ERRSTR_CANTINITDECODER);
		return false;
	}

	if (tinyjpeg_parse_header(pDec, pEncMsg->getImageData(), pEncMsg->getDataLength()) < 0)
	{
		// TODO: Ignore frame?
		tinyjpeg_free(pDec);
		return true;
	}

	unsigned int width, height;

	tinyjpeg_get_size(pDec, &width, &height);

	if (width%2 != 0 || height%2 != 0)
	{
		// TODO: Ignore frame?
		tinyjpeg_free(pDec);
		return true;
	}
	
	int allocSize = (width*height*3)/2;
	uint8_t *pImageBuffer = new uint8_t[allocSize];
	uint8_t *pPlanes[3];

	pPlanes[0] = pImageBuffer;
	pPlanes[1] = pPlanes[0]+width*height;
	pPlanes[2] = pPlanes[1]+(width*height)/4;

	tinyjpeg_set_components(pDec, pPlanes, 3);

	if (tinyjpeg_decode(pDec, TINYJPEG_FMT_YUV420P) < 0)
	{
		// TODO: Ignore frame?

		delete [] pImageBuffer;
		
		pPlanes[0] = 0;
		pPlanes[1] = 0;
		pPlanes[2] = 0;
		tinyjpeg_set_components(pDec, pPlanes, 3);
		tinyjpeg_free(pDec);
		return true;
	}

	pPlanes[0] = 0;
	pPlanes[1] = 0;
	pPlanes[2] = 0;
	tinyjpeg_set_components(pDec, pPlanes, 3);
	tinyjpeg_free(pDec);

	MIPRawYUV420PVideoMessage *pVideoMsg = new MIPRawYUV420PVideoMessage((int)width, (int)height, pImageBuffer, true);

	pVideoMsg->copyMediaInfoFrom(*pEncMsg); // copy time and sourceID
	m_messages.push_back(pVideoMsg);
		
	m_msgIt = m_messages.begin();

	return true;
}

bool MIPTinyJPEGDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPTINYJPEGDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
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

void MIPTinyJPEGDecoder::clearMessages()
{
	std::list<MIPVideoMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}


