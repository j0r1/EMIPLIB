/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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

#ifndef MIPJACKOUTPUT_H

#define MIPJACKOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_JACK

#include "mipcomponent.h"
#include "miptime.h"
#include <jack/jack.h>
#include <jmutex.h>
#include <string>

/** A Jack audio connection kit output component.
 *  This component uses the Jack audio connection kit to provide audio output.
 *  It accepts stereo raw floating point audio messages and does not produce
 *  any messages itself.
 */
class MIPJackOutput : public MIPComponent
{
public:
	MIPJackOutput();
	~MIPJackOutput();

	/** Initializes the output component.
	 *  Initializes the output component.
	 *  \param interval Output is processed in blocks of this size.
	 *  \param serverName Jack server name to which the client should connect.
	 *  \param arrayTime The amount of memory allocated to internal buffers,
	 *                   specified as a time interval. Note that this does not correspond
	 *                   to the amount of buffering introduced by this component.
	 */
	bool open(MIPTime interval = MIPTime(0.020), const std::string &serverName = std::string(""), MIPTime arrayTime = MIPTime(10.0));

	/** Cleans up the component. */
	bool close();

	/** Returns the sampling rate which is used by the component.
	 *  Returns the sampling rate which is used by the component. Incoming raw audio messages
	 *  should have this sampling rate.
	 */
	int getSamplingRate() const								{ if (m_pClient == 0) return 0; return m_sampRate; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	static int StaticJackCallback(jack_nframes_t nframes, void *arg);
	int JackCallback(jack_nframes_t nframes);
	
	jack_client_t *m_pClient;
	jack_port_t *m_pLeftOutput;
	jack_port_t *m_pRightOutput;
	
	int m_sampRate;
	float *m_pFrameArrayLeft;
	float *m_pFrameArrayRight;
	size_t m_frameArrayLength;
	size_t m_currentPos, m_nextPos;
	size_t m_blockLength, m_intervalLength;
	MIPTime m_delay;
	MIPTime m_distTime, m_blockTime, m_interval;
	JMutex m_frameMutex;
};

#endif // MIPCONFIG_SUPPORT_JACK

#endif // MIPJACKOUTPUT_H

