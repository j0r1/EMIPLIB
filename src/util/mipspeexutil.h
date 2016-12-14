/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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
 * \file mipspeexutil.h
 */

#ifndef MIPSPEEXUTIL_H

#define MIPSPEEXUTIL_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "miperrorbase.h"
#include <speex/speex.h>

/** An instance of this class can be used to analyse Speex data. */
class MIPSpeexUtil : public MIPErrorBase
{
public:
	/** Identifies the Speex bandwidth used. */
	enum SpeexBandWidth 
	{ 
		NarrowBand,		/**< Narrow band mode. */
		WideBand,		/**< Wide band mode. */
		UltraWideBand,		/**< Ultra wide band mode. */
		Invalid			/**< Didn't recognize the mode which was used. */
	};

	MIPSpeexUtil() 										{ m_bandWidth = Invalid; m_sampRate = 0; speex_bits_init(&m_bits); }
	~MIPSpeexUtil()										{ speex_bits_destroy(&m_bits); }

	/** Analyze the data given by \c speexData, of length \c length. */
	bool processBytes(const void *speexData, size_t length);

	/** Returns the bandwidth used by the previously processed speex data. */
	SpeexBandWidth getBandWidth() const							{ return m_bandWidth; }

	/** Returns the sampling rate used by the previously processed speex data. */
	int getSamplingRate() const								{ return m_sampRate; }
private:
	SpeexBits m_bits;
	SpeexBandWidth m_bandWidth;
	int m_sampRate;
};

#endif // MIPCONFIG_SUPPORT_SPEEX

#endif // MIPSPEEXUTIL_H

