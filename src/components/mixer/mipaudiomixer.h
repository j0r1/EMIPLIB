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
 * \file mipaudiomixer.h
 */

#ifndef MIPAUDIOMIXER_H

#define MIPAUDIOMIXER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>
#include <set>

class MIPRaw16bitAudioMessage;
class MIPRawFloatAudioMessage;

/** This component can mix several audio streams.
 *  Using this component, several audio streams can be mixed. In the default mode, it accepts 
 *  floating point raw audio messages and produces floating point raw audio messages. You
 *  can also work with signed 16 bit native raw audio messages. This component
 *  generates feedback about the current offset in the output stream.
 */
class EMIPLIB_IMPORTEXPORT MIPAudioMixer : public MIPComponent
{
public:
	MIPAudioMixer();
	~MIPAudioMixer();

	/** Initializes the mixer component.
	 *  Using this function, the audio mixer component can be initialized.
	 *  \param sampRate The sampling rate of incoming and outgoing audio messages.
	 *  \param channels The number of channels of incoming and outgoing audio messages.
	 *  \param blockTime Audio messages created by this component contain the amount of
	 *                   data corresponding to the specified time interval in \c blockTime
	 *  \param useTimeInfo If set to \c true, the time information in a raw audio message
	 *                     will be used to determine the position of the audio in the output
	 *                     stream. If set to \c false, incoming messages will be inserted
	 *                     at the head of the stream and timing information will be ignored.
	 *  \param floatSamples Flag indicating if floating point samples should be used or
	 *                      signed 16 bit native endian samples.
	 */
	bool init(int sampRate, int channels, MIPTime blockTime, bool useTimeInfo = true, bool floatSamples = true);

	/** De-initializes the mixer component.
	 *  This function frees the resources claimed by the mixer component.
	 */
	bool destroy();

	/** Adds an additional playback delay.
	 *  Using this function, an additional playback delay can be introduced. Note that only
	 *  positive delays are allowed. This can be useful for inter-media synchronization, in
	 *  case not all the component delays are known well enough to provide exact synchronization.
	 *  Using this function, the synchronization can then be adjusted manually.
	 */
	bool setExtraDelay(MIPTime t);

	/** Sets the internal playback time to a specific value.
	 *  Sets the internal playback time to a specific value. This allows multiple
	 *  audio mixers to be nicely synchronized (can come in handy when creating an
	 *  audio mixing server).
	 */
	void setPlaybackTime(MIPTime t)								{ m_playTime = t; }

	/** Returns the current internal playback time of this mixer. */
	MIPTime getPlaybackTime() const								{ return m_playTime; }

	/** Adds a source identifier which should be ignored. */
	void addSourceToIgnore(uint64_t id)							{ m_sourcesToIgnore.insert(id); }

	/** Clears the list of sources to ignore. */
	void clearIgnoreList()									{ m_sourcesToIgnore.clear(); }
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	bool processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback);
private:
	class MIPAudioMixerBlock
	{
	public:
		MIPAudioMixerBlock(int64_t playInterval, float *pFrames)			{ m_interval = playInterval; m_pFloatFrames = pFrames; m_pIntFrames = 0; }
		MIPAudioMixerBlock(int64_t playInterval, uint16_t *pFrames)			{ m_interval = playInterval; m_pFloatFrames = 0; m_pIntFrames = pFrames; }
		float *getFramesFloat()								{ return m_pFloatFrames; }
		uint16_t *getFramesInt()							{ return m_pIntFrames; }
		int64_t getInterval() const							{ return m_interval; }
	private:
		float *m_pFloatFrames;
		uint16_t *m_pIntFrames;
		int64_t m_interval;
	};
	
	void clearAudioBlocks();
	void initBlockSearch();
	MIPAudioMixerBlock searchBlock(int64_t intervalNumber);
	
	bool m_init;
	bool m_useTimeInfo;
	int64_t m_prevIteration;
	bool m_gotMessage;
	bool m_floatSamples;

	MIPTime m_blockTime, m_playTime;
	int m_sampRate, m_channels;
	size_t m_blockFrames,m_blockSize;
	int64_t m_curInterval;
	
	MIPRawFloatAudioMessage *m_pMsgFloat;
	MIPRaw16bitAudioMessage *m_pMsgInt;
	float *m_pSilenceFramesFloat;
	uint16_t *m_pSilenceFramesInt;
	float *m_pBlockFramesFloat;
	uint16_t *m_pBlockFramesInt;

	MIPTime m_extraDelay;
	
	std::list<MIPAudioMixerBlock> m_audioBlocks;
	std::list<MIPAudioMixerBlock>::iterator m_it;

	std::set<uint64_t> m_sourcesToIgnore;
};

#endif // MIPAUDIOMIXER_H

