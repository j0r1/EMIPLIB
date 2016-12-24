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
 * \file miprtpopusencoder.h
 */

#ifndef MIPRTPOPUSENCODER_H

#define MIPRTPOPUSENCODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_OPUS

#include "miprtpencoder.h"
#include <list>

class MIPRTPSendMessage;

/** Creates RTP packets for Opus compressed audio packets.
 *  This component accepts incoming Opus compressed audio packets and generates 
 *  MIPRTPSendMessage objects which can then be transferred to a MIPRTPComponent instance.
 */
class EMIPLIB_IMPORTEXPORT MIPRTPOpusEncoder : public MIPRTPEncoder
{
public:
	MIPRTPOpusEncoder();
	~MIPRTPOpusEncoder();

	/** Initializes the encoder. */
	bool init();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
private:
	void cleanUp();

	bool m_init;
};

#endif // MIPCONFIG_SUPPORT_OPUS

#endif // MIPRTPOPUSENCODER_H

