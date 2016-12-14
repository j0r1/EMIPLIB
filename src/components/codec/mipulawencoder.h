/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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
 * \file mipulawencoder.h
 */

#ifndef MIPULAWENCODER_H

#define MIPULAWENCODER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>

class MIPEncodedAudioMessage;

/** An u-law encoder.
 *  This component accepts raw audio messages using 16 bit signed native endian
 *  encoding. The samples are converted to u-law encoded samples and a message
 *  with type MIPMESSAGE_TYPE_AUDIO_ENCODED and subtype MIPENCODEDAUDIOMESSAGE_TYPE_ULAW
 *  is produced.
 */
class MIPULawEncoder : public MIPComponent
{
public:
	MIPULawEncoder();
	~MIPULawEncoder();

	/** Initialize the component. */
	bool init();

	/** Clean up the component. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();
	
	bool m_init;
	int64_t m_prevIteration;

	std::list<MIPEncodedAudioMessage *> m_messages;
	std::list<MIPEncodedAudioMessage *>::const_iterator m_msgIt;

	static const uint8_t expEnc[256];
};	

#endif // MIPULAWENCODER_H

