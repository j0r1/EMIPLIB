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
 * \file mipfrequencygenerator.h
 */

#ifndef MIPFREQUENCYGENERATOR_H

#define MIPFREQUENCYGENERATOR_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"

class MIPRawFloatAudioMessage;

/** Generates sound with specific frequencies.
 *  This component generates stereo sound, containing a signal with a specific 
 *  frequency in each channel. It takes system messages with subtype
 *  MIPSYSTEMMESSAGE_TYPE_ISTIME as input and generates stereo floating point
 *  raw audio messages.
 */
class MIPFrequencyGenerator : public MIPComponent
{
public:
	MIPFrequencyGenerator();
	~MIPFrequencyGenerator();

	/** Initializes the frequency generator.
	 *  This function is used to initialize the frequency generator.
	 *  \param leftFreq The frequency of the left channel.
	 *  \param rightFreq The frequency of the right channel.
	 *  \param leftAmp Amplitude of signal in the left channel.
	 *  \param rightAmp Amplitude of signal in the right channel.
	 *  \param sampRate The sampling rate of the output messages.
	 *  \param interval The time inverval contained in the output messages.
	 */
	bool init(real_t leftFreq, real_t rightFreq, real_t leftAmp, real_t rightAmp, int sampRate, MIPTime interval);
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void cleanUp();

	float *m_pFrames;
	int m_numFrames;
	MIPRawFloatAudioMessage *m_pAudioMsg;
	bool m_gotMessage;

	real_t m_leftFreq, m_rightFreq;
	real_t m_leftAmp, m_rightAmp;
	real_t m_curTime, m_timePerSample;
};

#endif // MIPFREQUENCYGENERATOR_H

