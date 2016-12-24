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

/**
 * \file mipsystemmessage.h
 */

#ifndef MIPSYSTEMMESSAGE_H

#define MIPSYSTEMMESSAGE_H

#include "mipconfig.h"
#include "mipmessage.h"

/**
 * \def MIPSYSTEMMESSAGE_TYPE_WAITTIME
 *      \brief A system message with this subtype is sent by the component chain to the first
 *             component to instuct it to wait until it is time to process the rest of the
 *             chain.
 * \def MIPSYSTEMMESSAGE_TYPE_ISTIME
 *      \brief This message can be sent by a timing component to the next component to let
 *             it know that an interval has elapsed.
 */

#define MIPSYSTEMMESSAGE_TYPE_WAITTIME				0x00000001
#define MIPSYSTEMMESSAGE_TYPE_ISTIME				0x00000002

/** A system message.
 *  This kind of message is used to instruct a component to wait until messages can be
 *  distributed in the chain or to inform a component that an interval has elapsed.
 */
class EMIPLIB_IMPORTEXPORT MIPSystemMessage : public MIPMessage
{
public:
	/** Constructs a system message with a specific subtype.
	 *  This constructor creates a system message with a specific subtype. These subtypes
	 *  can be found in mipsystemmessage.h
	 */
	MIPSystemMessage(uint32_t subtype) : MIPMessage(MIPMESSAGE_TYPE_SYSTEM, subtype)		{ }
};

#endif // MIPSYSTEMMESSAGE_H

