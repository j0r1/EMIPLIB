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

/**
 * \file mipmediamessage.h
 */

#ifndef MIPMEDIAMESSAGE_H

#define MIPMEDIAMESSAGE_H

#include "mipconfig.h"
#include "mipmessage.h"
#include "miptime.h"

/** Base class for media messages (like audio and video).
 *  This class is the base class for media messages (currently only for audio and video).
 *  Methods are provided to store timing information and a source identifier. Input
 *  components should store the sampling instant of the first sample in the timing
 *  information. The MIPRTPDecoder class uses the timing information to store the position
 *  of the data in the output stream.
 */
class MIPMediaMessage : public MIPMessage
{
public:
	/** Medium type. */
	enum MediumType { Audio, Video };
protected:
	/** Constructor, intended to be used by subclasses only. */
	/** With this constructor, a MIPMediaMessage instance can be created. It should
	 *  only be used by subclasses.
	 *  \param mType Medium type.
	 *  \param isRaw Flag indicating if the data contained in the message corresponds
	 *               to raw data or encoded (compressed) data.
	 *  \param msgSubtype The subtype of the message.
	 */
	MIPMediaMessage(MediumType mType, bool isRaw, uint32_t msgSubtype) : MIPMessage(calcType(mType,isRaw),msgSubtype), m_time(0)
														{ m_sourceID = 0; }
public:
	~MIPMediaMessage()											{ }

	/** Returns the source ID. */
	uint64_t getSourceID() const										{ return m_sourceID; }

	/** Returns the timing information. */
	MIPTime getTime() const											{ return m_time; }

	/** Sets the source ID. */
	void setSourceID(uint64_t id)										{ m_sourceID = id; }

	/** Sets the timing information. */
	void setTime(MIPTime t)											{ m_time = t; }

	/** Copies the source ID and timing information from another message. */
	void copyMediaInfoFrom(const MIPMediaMessage &msg)							{ m_sourceID = msg.m_sourceID; m_time = msg.m_time; }

	/** Virtual function to allow copies of subclass instances to be made. */
	virtual MIPMediaMessage *createCopy() const								{ return 0; }
private:
	static inline uint32_t calcType(MediumType mType, bool isRaw)
	{
		if (mType == Audio)
		{
			if (isRaw)
				return MIPMESSAGE_TYPE_AUDIO_RAW;
			return MIPMESSAGE_TYPE_AUDIO_ENCODED;
		}
		if (isRaw)
			return MIPMESSAGE_TYPE_VIDEO_RAW;
		return MIPMESSAGE_TYPE_VIDEO_ENCODED;
	}
			
	uint64_t m_sourceID;
	MIPTime m_time;
};

#endif // MIPMEDIAMESSAGE_H

