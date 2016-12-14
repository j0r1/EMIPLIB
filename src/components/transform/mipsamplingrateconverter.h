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
 * \file mipsamplingrateconverter.h
 */

#ifndef MIPSAMPLINGRATECONVERTER_H

#define MIPSAMPLINGRATECONVERTER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include <list>

class MIPAudioMessage;

/** Converts sampling rate and number of channels of raw audio messages.
 *  This component accepts incoming floating point or 16 bit signed integer raw audio 
 *  messages and produces
 *  similar messages with a specific sampling rate and number of channels set during
 *  initialization.
 */
class MIPSamplingRateConverter : public MIPComponent
{
public:
	MIPSamplingRateConverter();
	~MIPSamplingRateConverter();

	/** Initialize the sampling rate converter.
	 *  This function instructs the converter to generate raw audio 
	 *  messages with sampling rate \c outRate and number of channels \c outChannels.
	 *  If the \c floatSamples flag is set, floating point samples will be used,
	 *  otherwise 16 bit signed native endian samples will be used.
	 */
	bool init(int outRate, int outChannels, bool floatSamples = true);
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();
	
	bool m_init;
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
	int m_outRate, m_outChannels;
	bool m_floatSamples;
};

#endif // MIPSAMPLINGRATECONVERTER_H

