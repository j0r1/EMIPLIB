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
#include "mipyuv420framecutter.h"
#include "miprawvideomessage.h"

#include "mipdebug.h"

#define MIPYUV420FRAMECUTTER_ERRSTR_BADMESSAGE				"Only raw YUV420P messages with a specific height and width are accepted"
#define MIPYUV420FRAMECUTTER_ERRSTR_ALREADYINITIALIZED			"This component is already initialized"
#define MIPYUV420FRAMECUTTER_ERRSTR_BADINPUTDIMENSIONS			"Invalid input dimensions"
#define MIPYUV420FRAMECUTTER_ERRSTR_INPUTDIMENSIONSNOTEVEN		"Input dimensions should both be even numbers"
#define MIPYUV420FRAMECUTTER_ERRSTR_BADCUTCOORDINATES			"Invalid cut coordinates"
#define MIPYUV420FRAMECUTTER_ERRSTR_CUTCOORDNOTEVEN			"The specified cut coordinates are not even"
#define MIPYUV420FRAMECUTTER_ERRSTR_NOTINITIALIZED			"This component has not yet been initialized"

MIPYUV420FrameCutter::MIPYUV420FrameCutter() : MIPComponent("MIPYUV420FrameCutter")
{
	m_init = false;
}

MIPYUV420FrameCutter::~MIPYUV420FrameCutter()
{
	destroy();
}

bool MIPYUV420FrameCutter::init(int inputWidth, int inputHeight, int x0, int x1, int y0, int y1)
{
	if (m_init)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_ALREADYINITIALIZED);
		return false;
	}

	if (inputWidth < 1 || inputHeight < 1)	
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_BADINPUTDIMENSIONS);
		return false;
	}

	if (inputWidth%2 != 0 || inputHeight%2 != 0)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_INPUTDIMENSIONSNOTEVEN);
		return false;
	}

	if (x0 >= x1 || y0 >= y1 || x0 < 0 || x1 > inputWidth || y0 < 0 || y1 > inputHeight)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_BADCUTCOORDINATES);
		return false;
	}

	if (x0%2 != 0 || x1%2 != 0 || y0%2 != 0 || y1%2 != 0)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_CUTCOORDNOTEVEN);
		return false;
	}
	
	m_inputWidth = inputWidth;
	m_inputHeight = inputHeight;
	m_x0 = x0;
	m_x1 = x1;
	m_y0 = y0;
	m_y1 = y1;

	m_lastIteration = -1;
	m_init = true;
	m_msgIt = m_messages.begin();
	return true;
}

bool MIPYUV420FrameCutter::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	clearMessages();

	m_init = false;

	return true;
}

bool MIPYUV420FrameCutter::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	if (m_lastIteration != iteration)
	{
		clearMessages();
		m_lastIteration = iteration;
	}

	if (pMsg->getMessageType() != MIPMESSAGE_TYPE_VIDEO_RAW)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_BADMESSAGE);
		return false;
	}

	uint32_t subType = pMsg->getMessageSubtype();
	if (subType != MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	MIPRawYUV420PVideoMessage *pInputMsg = (MIPRawYUV420PVideoMessage *)pMsg;

	if (pInputMsg->getWidth() != m_inputWidth || pInputMsg->getHeight() != m_inputHeight)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_BADMESSAGE);
		return false;
	}

	int dstWidth = m_x1-m_x0;
	int dstHeight = m_y1-m_y0;
	int dstSize = (dstWidth*dstHeight*3)/2;
	uint8_t *pData = new uint8_t[dstSize];
	const uint8_t *pInputData = pInputMsg->getImageData();

	MIPRawYUV420PVideoMessage *pNewMsg = new MIPRawYUV420PVideoMessage(dstWidth, dstHeight, pData, true);

	int dstOffset = 0;
	int srcPos = m_x0+m_y0*m_inputWidth;

	for (int y = m_y0 ; y < m_y1 ; y++, dstOffset += dstWidth, srcPos += m_inputWidth)
		memcpy(pData+dstOffset, pInputData+srcPos, dstWidth);

	int y0 = m_y0/2;
	int y1 = m_y1/2;
	
	dstOffset = dstWidth*dstHeight;
	int dstOffset2 = dstOffset + dstOffset/4;

	dstWidth /= 2;
	int srcWidth = m_inputWidth/2;
	int inputPixels = m_inputWidth*m_inputHeight;
	srcPos = inputPixels + m_x0/2 + (m_y0/2)*srcWidth;
	int srcPos2 = srcPos + inputPixels/4;

	for (int y = y0 ; y < y1 ; y++, dstOffset += dstWidth, dstOffset2 += dstWidth, srcPos += srcWidth, srcPos2 += srcWidth)
	{
		memcpy(pData+dstOffset, pInputData+srcPos, dstWidth);
		memcpy(pData+dstOffset2, pInputData+srcPos2, dstWidth);
	}

	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();

	return true;
}

bool MIPYUV420FrameCutter::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPYUV420FRAMECUTTER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	if (m_lastIteration != iteration)
	{
		clearMessages();
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

void MIPYUV420FrameCutter::clearMessages()
{
	std::list<MIPVideoMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

