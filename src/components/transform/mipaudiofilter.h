/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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
 * \file mipaudiofilter.h
 */

#ifndef MIPAUDIOFILTER_H

#define MIPAUDIOFILTER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miprawaudiomessage.h"
#include "miptime.h"
#include <list>

class MIPAudioMessage;

/** Removes a frequency range from raw floating point audio.
 *  Removes a frequency range from audio messages.
 *  It accepts floating point raw audio messages and produces similar audio messages.
 */
class MIPAudioFilter : public MIPComponent
{
public:
	MIPAudioFilter();
	~MIPAudioFilter();

	/** Initializes the filter. 
	 *  Initializes the audio filter.
	 *  \param sampRate Prepares buffers and calculations for audio with this sampling rate.
	 *  \param channels The number of channels in the audio messages.
	 *  \param interval The time interval contained in each audio message.
	 */
	bool init(int sampRate, int channels, MIPTime interval);

	/** This will remove frequencies below \c frequency from the audio messages. */
	void setLowFilter(float frequency)						{ m_lowFreq = frequency; m_useLow = true; }

	/** This will remove frequencies above \c frequency from the audio messages. */
	void setHighFilter(float frequency)						{ m_highFreq = frequency; m_useHigh = true; }

	/** This will remove frequencies between \c lowFrequency and \c highFrequency from the audio messages. */
	void setMiddleFilter(float lowFrequency, float highFrequency)			{ m_midLowFreq = lowFrequency; m_midHighFreq = highFrequency; m_useMiddle = true; }

	/** Disables the low frequency filter. */
	void clearLowFilter()								{ m_useLow = false; }

	/** Disables the high frequency filter. */
	void clearHighFilter()								{ m_useHigh = false; }

	/** Disables the frequency range filter. */
	void clearMiddleFilter()							{ m_useMiddle = false; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();
	void clearMessages();

	bool m_useLow, m_useHigh, m_useMiddle;
	float m_lowFreq, m_highFreq, m_midLowFreq, m_midHighFreq;
	
	bool m_init;
	int m_sampRate, m_channels;
	int m_audioSize, m_numFrames;
	float *m_pAM, *m_pBM;
	float *m_pA, *m_pB;
	float *m_pBuf;
	float m_period;
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPAUDIOFILTER_H

