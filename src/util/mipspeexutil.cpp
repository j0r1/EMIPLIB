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

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "miptypes.h"
#include "mipspeexutil.h"

#include "mipdebug.h"

bool MIPSpeexUtil::processBytes(const void *speexData, size_t length)
{
	m_bandWidth = Invalid;
	m_sampRate = 0;

	speex_bits_read_from(&m_bits, (char *)speexData, (int)length);

	if (speex_bits_remaining(&m_bits) < 5) // invalid frame
	{
		// TODO
		return false;
	}
	
	int wideband = speex_bits_unpack_unsigned(&m_bits, 1);
	if (wideband)
	{
		// First part of the frame should be narrowband!
		// TODO: error message
		return false;
	}

	int mode = speex_bits_unpack_unsigned(&m_bits,4);
	int advance = mode;

	speex_mode_query(&speex_nb_mode, SPEEX_SUBMODE_BITS_PER_FRAME, &advance);
	if (advance < 0)
	{
		// TODO
		return false;
	}

	advance -= 5;
	speex_bits_advance(&m_bits, advance);
	
	if (speex_bits_remaining(&m_bits) == 0) // ok, out of bits
	{
		m_bandWidth = NarrowBand;
		m_sampRate = 8000;
		return true;
	}

	if (speex_bits_remaining(&m_bits) < 4) // invalid frame
	{
		// TODO
		return false;
	}
	
	wideband = speex_bits_unpack_unsigned(&m_bits, 1);
	if (!wideband)
	{
		// assume its the start of another frame
		m_bandWidth = NarrowBand;
		m_sampRate = 8000;
		return true;
	}

	mode = speex_bits_unpack_unsigned(&m_bits, 3);
	advance = mode;
	
	speex_mode_query(&speex_wb_mode, SPEEX_SUBMODE_BITS_PER_FRAME, &advance);
	if (advance < 0)
	{
		// TODO
		return false;
	}
	
	advance -= 4;
	speex_bits_advance(&m_bits, advance);

	if (speex_bits_remaining(&m_bits) == 0) // ok, out of bits
	{
		m_bandWidth = WideBand;
		m_sampRate = 16000;
		return true;
	}

	if (speex_bits_remaining(&m_bits) < 4) // invalid frame
	{
		// TODO
		return false;
	}
	
	wideband = speex_bits_unpack_unsigned(&m_bits, 1);
	if (!wideband)
	{
		// assume its the start of another frame
		m_bandWidth = WideBand;
		m_sampRate = 16000;
		return true;
	}

	mode = speex_bits_unpack_unsigned(&m_bits, 3);
	advance = mode;
	
	speex_mode_query(&speex_wb_mode, SPEEX_SUBMODE_BITS_PER_FRAME, &advance);
	if (advance < 0)
	{
		// TODO
		return false;
	}
	
	advance -= 4;
	speex_bits_advance(&m_bits, advance);

	if (speex_bits_remaining(&m_bits) == 0) // ok, out of bits
	{
		m_bandWidth = UltraWideBand;
		m_sampRate = 32000;
		return true;
	}

	if (speex_bits_remaining(&m_bits) < 4) // invalid frame
	{
		// TODO
		return false;
	}
	
	wideband = speex_bits_unpack_unsigned(&m_bits, 1);
	if (!wideband)
	{
		// assume its the start of another frame
		m_bandWidth = UltraWideBand;
		m_sampRate = 32000;
		return true;
	}

	// too many wide band frames! invalid frame
	// TODO: error message
	
	return false;
}

#endif // MIPCONFIG_SUPPORT_SPEEX

