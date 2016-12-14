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
 * \file mipvideomessage.h
 */

#ifndef MIPVIDEOMESSAGE_H

#define MIPVIDEOMESSAGE_H

#include "mipconfig.h"
#include "mipmediamessage.h"
#include "miptypes.h"
#include "miptime.h"

/** Base class for encoded and raw video messages.
 *  This class is the base class of both raw video messages and encoded video messages.
 */
class MIPVideoMessage : public MIPMediaMessage
{
protected:
	/** Constructor of the class.
	 *  Constructor of the class, meant to be used by subclasses.
	 *  \param isRaw Flag indicating is the message is intended for raw video frames or
	 *               encoded video.
	 *  \param msgSubtype Subtype of the message.
	 *  \param width Width of the video frame.
	 *  \param height Height of the video frame.
	 */
	MIPVideoMessage(bool isRaw, uint32_t msgSubtype, int width, int height) : MIPMediaMessage(MIPMediaMessage::Video, isRaw, msgSubtype)
												{ m_height = height; m_width = width; }
public:
	~MIPVideoMessage()									{ }

	/** Returns the width of the video frame. */
	int getWidth() const									{ return m_width; }

	/** Returns the height of the video frame. */
	int getHeight() const									{ return m_height; }

	/** Copies the information stored in \c msg, including the MIPMediaMessage info. */
	void copyVideoInfoFrom(const MIPVideoMessage &msg)					{ m_width = msg.m_width; m_height = msg.m_height; }
private:
	int m_width, m_height;
};

#endif // MIPVIDEOMESSAGE_H

