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
 * \file mipencodedaudiomessage.h
 */

#ifndef MIPENCODEDAUDIOMESSAGE_H

#define MIPENCODEDAUDIOMESSAGE_H

#include "mipconfig.h"
#include "mipaudiomessage.h"
#include "miptime.h"
#include <string.h>

/**
 * \def MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX
 * 	\brief Subtype for encoded audio messages containing Speex data.
 * \def MIPENCODEDAUDIOMESSAGE_TYPE_ULAW
 * 	\brief Subtype for encoded audio messages containing u-law data.
 * \def MIPENCODEDAUDIOMESSAGE_TYPE_ALAW
 * 	\brief Subtype for encoded audio messages containing A-law data.
 * \def MIPENCODEDAUDIOMESSAGE_TYPE_GSM
 * 	\brief Subtype for encoded audio messages containing GSM 06.10 data.
 * \def MIPENCODEDAUDIOMESSAGE_TYPE_LPC
 * 	\brief Subtype for encoded audio messages containing LPC data.
*/

#define MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX							0x00000001
#define MIPENCODEDAUDIOMESSAGE_TYPE_ULAW							0x00000002
#define MIPENCODEDAUDIOMESSAGE_TYPE_ALAW							0x00000004
#define MIPENCODEDAUDIOMESSAGE_TYPE_GSM								0x00000008
#define MIPENCODEDAUDIOMESSAGE_TYPE_LPC								0x00000010

/** Container for encoded audio data.
 */
class MIPEncodedAudioMessage : public MIPAudioMessage
{
public:
	/** Creates an encoded audio message.
	 *  This constructor can be used to create an encoded audio message.
	 *  \param subType The subtype of the message.
	 *  \param samplingRate The sampling rate.
	 *  \param numChannels The number of channels.
	 *  \param numFrames The number of frames contained in the message.
	 *  \param pData The actual encoded audio data.
	 *  \param numBytes The length of the encoded audio data.
	 *  \param deleteData Flag indicating if the memory block pointed to by \c pData should
	 *                    be deleted when this message is destroyed.
	 */
	MIPEncodedAudioMessage(uint32_t subType, int samplingRate, int numChannels, 
	                       int numFrames, uint8_t *pData, size_t numBytes, bool deleteData) : MIPAudioMessage(false, subType, samplingRate, numChannels, numFrames)
													{ m_deleteData = deleteData; m_pData = pData; m_dataLength = numBytes; }
	~MIPEncodedAudioMessage()									{ if (m_deleteData) delete [] m_pData; }

	/** Returns a pointer to the encoded audio data. */
	const uint8_t *getData() const									{ return m_pData; }

	/** Returns the length of the encoded audio. */
	size_t getDataLength() const									{ return m_dataLength; }

	/** Sets the length of the encoded audio to 'l'. */
	void setDataLength(size_t l)									{ m_dataLength = l; }

	/** Creates a copy of this message. */
	MIPMediaMessage *createCopy() const;
private:
	bool m_deleteData;
	uint8_t *m_pData;
	size_t m_dataLength;
};

inline MIPMediaMessage *MIPEncodedAudioMessage::createCopy() const
{
	uint8_t *pBytes = new uint8_t [m_dataLength];

	memcpy(pBytes, m_pData, m_dataLength);
	MIPMediaMessage *pMsg = new MIPEncodedAudioMessage(getMessageSubtype(), getSamplingRate(),
			                                   getNumberOfChannels(), getNumberOfFrames(),
							   pBytes, m_dataLength, true);
	pMsg->copyMediaInfoFrom(*this);
	return pMsg;
}

#endif // MIPENCODEDAUDIOMESSAGE_H

