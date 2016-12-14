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

/** 
 * \file mipmessage.h
 */

#ifndef MIPMESSAGE_H

#define MIPMESSAGE_H

#include "mipconfig.h"
#include "miptypes.h"
#ifdef MIPDEBUG
	#include <iostream>
#endif // MIPDEBUG

/**
 * \def MIPMESSAGE_TYPE_SYSTEM
 *      \brief A message with this type number indicates an object of type MIPSystemMessage.
 * \def MIPMESSAGE_TYPE_AUDIO_RAW
 *      \brief A message with this type number indicates a raw audio message (miprawaudiomessage.h).
 * \def MIPMESSAGE_TYPE_AUDIO_ENCODED
 * 	\brief A message with this type indicates an encoded audio message (mipencodedaudiomessage.h).
 * \def MIPMESSAGE_TYPE_VIDEO_RAW
 *      \brief A message with this type number indicates a raw video message (miprawvideomessage.h).
 * \def MIPMESSAGE_TYPE_VIDEO_ENCODED
 * 	\brief A message with this type indicates an encoded video message (mipencodedvideomessage.h).
 * \def MIPMESSAGE_TYPE_RTP
 *      \brief A message with this type number indicates either an object of type MIPRTPSendMessage
 * 	       or an object of type MIPRTPReceiveMessage.
 * \def MIPMESSAGE_TYPE_ALL
 *      \brief This define can be used in specifying a message filter to indicate that all messages
 *             types (or subtypes) are allowed.
 */

#define MIPMESSAGE_TYPE_SYSTEM								0x00000001
#define MIPMESSAGE_TYPE_AUDIO_RAW							0x00000002
#define MIPMESSAGE_TYPE_AUDIO_ENCODED							0x00000004
#define MIPMESSAGE_TYPE_VIDEO_RAW							0x00000008
#define MIPMESSAGE_TYPE_VIDEO_ENCODED							0x00000010
#define MIPMESSAGE_TYPE_RTP								0x00000020
#define MIPMESSAGE_TYPE_ALL								0xffffffff

/** Base class of messages passed in a MIPComponentChain instance.
 *  This is the base class of messages distributed in a MIPComponentChain
 *  instance. Messages are distributed from component to component using
 *  the MIPComponent::pull and MIPComponent::push functions. The
 *  message type numbers can be found in mipmessage.h
 */
class MIPMessage
{
protected:
	/** Creates a message with specific type and subtype.
	 */
	MIPMessage(uint32_t msgType, uint32_t msgSubtype)				{ m_msgType = msgType; m_msgSubtype = msgSubtype; }
public:
	virtual ~MIPMessage()								{ }

	/** Returns the message type.
	 */
	uint32_t getMessageType() const							{ return m_msgType; }

	/** Returns the message subtype.
	 */
	uint32_t getMessageSubtype() const						{ return m_msgSubtype; }
protected:
	/** Changes the subtype of the message.
	 */
	void setMessageSubtype(uint32_t s)						{ m_msgSubtype = s; }
private:
	uint32_t m_msgType, m_msgSubtype;
};

#endif // MIPMESSAGE_H


