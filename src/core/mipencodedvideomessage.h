/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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
 * \file mipencodedvideomessage.h
 */

#ifndef MIPENCODEDVIDEOMESSAGE_H

#define MIPENCODEDVIDEOMESSAGE_H

#include "mipconfig.h"
#include "mipvideomessage.h"
#include "miptypes.h"
#include "miptime.h"

/**
 * \def MIPENCODEDVIDEOMESSAGE_TYPE_H263P
 * 	\brief Subtype for encoded video message using H.263+ encoding.
 */

#define MIPENCODEDVIDEOMESSAGE_TYPE_H263P							0x00000001

/** Container for encoded video data. */
class MIPEncodedVideoMessage : public MIPVideoMessage
{
public:
	/** Creates an encoded video message.
	 *  Using this constructor encoded video messages can be created.
	 *  \param msgSubtype Message subtype.
	 *  \param width Width of the video frame.
	 *  \param height Hieght of the video frame.
	 *  \param pData The encoded video data.
	 *  \param dataLength The length of the encoded video data.
	 *  \param deleteData Flag indicating if the memory block pointed to by \c pData should
	 *                    be deleted when this message is destroyed.
	 */
	MIPEncodedVideoMessage(uint32_t msgSubtype, int width, int height, uint8_t *pData, size_t dataLength, bool deleteData) : MIPVideoMessage(false, msgSubtype, width, height)
												{ m_pData = pData; m_dataLength = dataLength; m_deleteData = deleteData; }
	~MIPEncodedVideoMessage()								{ if (m_deleteData) delete [] m_pData; }
	
	/** Returns the encoded image data. */
	const uint8_t *getImageData() const							{ return m_pData; }

	/** Returns the length of the encoded image data. */
	size_t getDataLength() const								{ return m_dataLength; }

	/** Sets the length of the encoded image data. */
	void setDataLength(size_t l)								{ m_dataLength = l; }

	/** Create a copy of this message. */
	MIPMediaMessage *createCopy() const
	{
		uint8_t *pData = new uint8_t [m_dataLength];
		memcpy(pData, m_pData, m_dataLength);
		MIPMediaMessage *pMsg = new MIPEncodedVideoMessage(getMessageSubtype(), getWidth(), getHeight(),
		                                                   pData, m_dataLength, true);
		pMsg->copyMediaInfoFrom(*this);
		return pMsg;
	}
private:
	bool m_deleteData;
	uint8_t *m_pData;
	size_t m_dataLength;
};

#endif // MIPENCODEDVIDEOMESSAGE_H

