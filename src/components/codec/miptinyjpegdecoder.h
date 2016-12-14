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
 * \file miptinyjpegdecoder.h
 */

#ifndef MIPTINYJPEGDECODER_H

#define MIPTINYJPEGDECODER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include <list>

class MIPVideoMessage;

/** A JPEG decoder based on the Tiny JPEG Decoder library.
 *  A JPEG decoder based on the Tiny JPEG Decoder library. It accepts frames that
 *  are JPEG compressed and outputs YUV420P frames.
 */
class EMIPLIB_IMPORTEXPORT MIPTinyJPEGDecoder : public MIPComponent
{
public:
	MIPTinyJPEGDecoder();
	~MIPTinyJPEGDecoder();

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

	std::list<MIPVideoMessage *> m_messages;
	std::list<MIPVideoMessage *>::const_iterator m_msgIt;
};

#endif // MIPTINYJPEGDECODER_H

