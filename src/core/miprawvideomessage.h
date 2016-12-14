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

/**
 * \file miprawvideomessage.h
 */

#ifndef MIPRAWVIDEOMESSAGE_H

#define MIPRAWVIDEOMESSAGE_H

#include "mipconfig.h"
#include "mipvideomessage.h"
#include "miptypes.h"
#include "miptime.h"
#include <string.h>

/**
 * \def MIPRAWVIDEOMESSAGE_TYPE_YUV420P
 * 	\brief A message with this subtype represents a YUV420P encoded video frame, stored in a
 *	       MIPRawYUV420PVideoMessage instance.
 * \def MIPRAWVIDEOMESSAGE_TYPE_YUYV
 * 	\brief A message with this subtype represents a YUYV encoded video frame, stored in a
 *	       MIPRawYUYVVideoMessage instance.
 */

#define MIPRAWVIDEOMESSAGE_TYPE_YUV420P								0x00000001
#define MIPRAWVIDEOMESSAGE_TYPE_YUYV								0x00000002

/** Container for an YUV420P encoded raw video frame. */
class MIPRawYUV420PVideoMessage : public MIPVideoMessage
{
public:
	/** Creates a raw video message with a YUV420P representation.
	 *  Creates a raw video message with a YUV420P representation.
	 *  \param width Width of the video frame.
	 *  \param height Height of the video frame.
	 *  \param pData The data of the video frame.
	 *  \param deleteData Flag indicating if the data contained in \c pData should be
	 *                    deleted when this message is destroyed or when the data is replaced.
	 */
	MIPRawYUV420PVideoMessage(int width, int height, uint8_t *pData, bool deleteData) : MIPVideoMessage(true, MIPRAWVIDEOMESSAGE_TYPE_YUV420P, width, height)
												{ m_pData = pData; m_deleteData = deleteData; }
	~MIPRawYUV420PVideoMessage()								{ if (m_deleteData) delete [] m_pData; }

	/** Returns the image data. */
	const uint8_t *getImageData() const							{ return m_pData; }

	/** Returns a copy of the message. */
	MIPMediaMessage *createCopy() const
	{
		size_t dataSize = (getWidth()*getHeight()*3)/2;
		uint8_t *pData = new uint8_t [dataSize];

		memcpy(pData, m_pData, dataSize);
		MIPMediaMessage *pMsg = new MIPRawYUV420PVideoMessage(getWidth(), getHeight(), pData, true);
		pMsg->copyMediaInfoFrom(*this);
		return pMsg;
	}
private:
	bool m_deleteData;
	uint8_t *m_pData;
};

/** Container for an YUYV encoded raw video frame. */
class MIPRawYUYVVideoMessage : public MIPVideoMessage
{
public:
	/** Creates a raw video message with a YUYV representation.
	 *  Creates a raw video message with a YUYV representation.
	 *  \param width Width of the video frame.
	 *  \param height Height of the video frame.
	 *  \param pData The data of the video frame.
	 *  \param deleteData Flag indicating if the data contained in \c pData should be
	 *                    deleted when this message is destroyed or when the data is replaced.
	 */
	MIPRawYUYVVideoMessage(int width, int height, uint8_t *pData, bool deleteData) : MIPVideoMessage(true, MIPRAWVIDEOMESSAGE_TYPE_YUYV, width, height)
												{ m_pData = pData; m_deleteData = deleteData; }
	~MIPRawYUYVVideoMessage()								{ if (m_deleteData) delete [] m_pData; }

	/** Returns the image data. */
	const uint8_t *getImageData() const							{ return m_pData; }

	/** Returns a copy of the message. */
	MIPMediaMessage *createCopy() const
	{
		size_t dataSize = getWidth()*getHeight()*2;
		uint8_t *pData = new uint8_t [dataSize];

		memcpy(pData, m_pData, dataSize);
		MIPMediaMessage *pMsg = new MIPRawYUYVVideoMessage(getWidth(), getHeight(), pData, true);
		pMsg->copyMediaInfoFrom(*this);
		return pMsg;
	}
private:
	bool m_deleteData;
	uint8_t *m_pData;
};

#endif // MIPRAWVIDEOMESSAGE_H

