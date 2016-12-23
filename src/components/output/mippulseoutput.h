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
 * \file mipjackoutput.h
 */

#ifndef MIPPULSEOUTPUT_H

#define MIPPULSEOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_PULSEAUDIO

#include "mipcomponent.h"
#include "miptime.h"
#include <pulse/simple.h>
#include <jthread/jmutex.h>
#include <string>

/** A PulseAudio output component.
 *  This component uses PulseAudio to provide audio output.
 *  It accepts raw floating point audio messages and does not produce
 *  any messages itself.
 */
class EMIPLIB_IMPORTEXPORT MIPPulseOutput : public MIPComponent
{
public:
	MIPPulseOutput();
	~MIPPulseOutput();

	/** Initializes the output component.
	 *  Initializes the output component.
	 *  \param sampleRate The sampling rate of the audio
	 *  \param channels The number of channels of the audio
	 *  \param interval Output is processed in blocks of this size.
	 *  \param serverName PulseAudio server name to which the client should connect.
	 */
	bool open(int sampleRate = 48000, int channels = 2, MIPTime interval = MIPTime(0.020),
	          const std::string &serverName = std::string(""));

	/** Cleans up the component. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:	
	pa_simple *m_pStream;
	
	int m_sampRate;
	int m_channels;
};

#endif // MIPCONFIG_SUPPORT_PULSEAUDIO

#endif // MIPPULSEOUTPUT_H

