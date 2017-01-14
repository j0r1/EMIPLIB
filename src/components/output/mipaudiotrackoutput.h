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
 * \file mipaudiotrackoutput.h
 */

#ifndef MIPAUDIOTRACKOUTPUT_H

#define MIPAUDIOTRACKOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_AUDIOTRACK

#include "mipcomponent.h"
#include "miptime.h"
#include <jthread/jmutex.h>
#include <string>

namespace android
{
	class AudioTrack;
};

/** Component which uses the AudioTrack class available on the Android
 *  platform to play back audio.
 *  This component uses the AudioTrack class available on the Android
 *  platform to play back audio. It accepts raw signed 16 bit audio
 *  messages, using native byte ordering. No output messages are
 *  produced.
 */
class EMIPLIB_IMPORTEXPORT MIPAudioTrackOutput : public MIPComponent
{
public:
	MIPAudioTrackOutput();
	~MIPAudioTrackOutput();

	/** Opens the output component.
	 *  Opens the output component.
	 *  \param sampRate The sampling rate (e.g. 8000, 22050, 44100, ...)
	 *  \param channels The number of channels (e.g. 1 for mono, 2 for stereo)
	 *  \param interval This interval is used to play back audio.
	 *  \param arrayTime Size of the internal buffer used to temporarily store audio in, expressed as a 
	 *                   time interval. Note that this is not the amount of delay introduced by the component.
	 */
	bool open(int sampRate, int channels, MIPTime interval = MIPTime(0.020), MIPTime arrayTime = MIPTime(10.0));

	/** Closes the output component. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	static void StaticAudioTrackCallback(int event, void *pUser, void *pInfo);
	void AudioTrackCallback(void *pInfo);
	
	android::AudioTrack *m_pTrack;

	int m_sampRate, m_channels;
	uint16_t *m_pFrameArray;
	size_t m_frameArrayLength;
	size_t m_currentPos, m_nextPos;
	size_t m_blockLength, m_blockFrames, m_intervalLength;
	MIPTime m_delay;
	MIPTime m_distTime, m_blockTime, m_interval;
	jthread::JMutex m_frameMutex;
};

#endif // MIPCONFIG_SUPPORT_AUDIOTRACK

#endif // MIPAUDIOTRACKOUTPUT_H

