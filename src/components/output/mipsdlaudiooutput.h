/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Hasselt University - Expertise Centre for
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
 * \file mipsdlaudiooutput.h
 */

#ifndef MIPSDLAUDIOOUTPUT_H

#define MIPSDLAUDIOOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SDLAUDIO

#include "mipcomponent.h"
#include "miptime.h"
#include <jmutex.h>
#include <string>

/** An SDL audio output component.
 *  This component uses the SDL audio system to produce audio output. It accepts
 *  16 bit native endian raw audio messages and produces no messages itself.
 */
class MIPSDLAudioOutput : public MIPComponent
{
public:
	MIPSDLAudioOutput();
	~MIPSDLAudioOutput();

	/** Initializes the component.
	 *  Initializes the component.
	 *  \param sampRate The sampling rate.
	 *  \param channels The number of channels.
	 *  \param interval Intenally, audio data is written to the soundcard device each time
	 *                  inteval elapses. Note that this value may be changed somewhat.
	 *  \param arrayTime The amount of memory allocated to internal buffers,
	 *                   specified as a time interval. Note that this does not correspond
	 *                   to the amount of buffering introduced by this component.
	*/
	bool open(int sampRate, int channels, MIPTime interval = MIPTime(0.020), MIPTime arrayTime = MIPTime(10.0));

	/** Closes a previously opened SDL audio component. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	static void StaticCallback(void *udata, uint8_t *stream, int len);
	void Callback(uint8_t *stream, int len);
	
	bool m_init;
	int m_sampRate;
	int m_channels;
	uint16_t *m_pFrameArray;
	size_t m_frameArrayLength;
	size_t m_currentPos, m_nextPos;
	size_t m_blockLength, m_intervalLength;
	MIPTime m_delay;
	MIPTime m_distTime, m_blockTime, m_interval;
	JMutex m_frameMutex;
};

#endif // MIPCONFIG_SUPPORT_SDLAUDIO

#endif // MIPSDLAUDIOOUTPUT_H

