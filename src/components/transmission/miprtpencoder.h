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
 * \file miprtpencoder.h
 */

#ifndef MIPRTPENCODER_H

#define MIPRTPENCODER_H

#include "mipconfig.h"
#include "mipcomponent.h"

/** Base class for RTP encoders.
 *  Base class for RTP encoders. Contains a member function to set the payload type.
 */
class MIPRTPEncoder : public MIPComponent
{
public:
	MIPRTPEncoder(const std::string &compName) : MIPComponent(compName)					{ m_payloadType = 0; }
	~MIPRTPEncoder()											{ }

	/** Sets the payload type. */
	void setPayloadType(uint8_t payloadType) 								{ m_payloadType = payloadType; }

	/** Returns the payload type. */
	uint8_t getPayloadType() const										{ return m_payloadType; }
private:
	uint8_t m_payloadType;
};

#endif // MIPRTPENCODER_H

