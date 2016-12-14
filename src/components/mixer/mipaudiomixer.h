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
 * \file mipaudiomixer.h
 */

#ifndef MIPAUDIOMIXER_H

#define MIPAUDIOMIXER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>

class MIPRawFloatAudioMessage;

/** This component can mix several audio streams.
 *  Using this component, several audio streams can be mixed. It accepts floating point
 *  raw audio messages and produces floating point raw audio messages. This component
 *  generates feedback about the current offset in the output stream.
 */
class MIPAudioMixer : public MIPComponent
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
	 */
	bool init(int sampRate, int channels, MIPTime blockTime, bool useTimeInfo = true);

	/** De-initializes the mixer component.
	 *  This function frees the resources claimed by the mixer component.
	 */
	bool destroy();

	/** Adds an additional playback delay.
	 *  Using this function, an additional playback delay can be introduced. Note that only
	 *  positive delays are allowed. This can be useful for inter-media synchronization, in
	 *  case not all the component delays are known well enough to provide exact synchronization.
	 *  Using this function, the synchronization can then be adjusted manually. If \c useTimeInfo
	 *  was set to false in the MIPAudioMixer::init function, this call has no effect.
	 */
	bool setExtraDelay(MIPTime t);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	bool processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback);
private:
	class MIPAudioMixerBlock
	{
	public:
		MIPAudioMixerBlock(int64_t playInterval, float *pFrames)			{ m_interval = playInterval; m_pFrames = pFrames; }
		float *getFrames()								{ return m_pFrames; }
		int64_t getInterval() const							{ return m_interval; }
	private:
		float *m_pFrames;
		int64_t m_interval;
	};
	
	void clearAudioBlocks();
	void initBlockSearch();
	MIPAudioMixerBlock searchBlock(int64_t intervalNumber);
	
	bool m_init;
	bool m_useTimeInfo;
	int64_t m_prevIteration;
	bool m_gotMessage;

	MIPTime m_blockTime, m_playTime;
	int m_sampRate, m_channels;
	size_t m_blockFrames,m_blockSize;
	int64_t m_curInterval;
	
	MIPRawFloatAudioMessage *m_pMsg;
	float *m_pSilenceFrames;
	float *m_pBlockFrames;

	MIPTime m_extraDelay;
	
	std::list<MIPAudioMixerBlock> m_audioBlocks;
	std::list<MIPAudioMixerBlock>::iterator m_it;
};

#endif // MIPAUDIOMIXER_H

