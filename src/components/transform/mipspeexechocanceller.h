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
 * \file mipspeexechocanceller.h
 */

#ifndef MIPSPEEXECHOCANCELLER_H

#define MIPSPEEXECHOCANCELLER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "mipcomponent.h"
#include "miprawaudiomessage.h"
#include <list>

/** An echo cancellation component based on the Speex echo cancellation routines.
 *  An echo cancellation component based on the Speex echo cancellation routines. After
 *  you've created this component, you should insert it into the recording part of the
 *  chain, so that it can inspect incoming messages and remove the echo from them.
 *  An extra component is automatically created and can be obtained using the
 *  MIPSpeexEchoCanceller::getOutputAnalyzer method. This component has to be inserted
 *  into the playback part of your chain. Both components only accept raw 16 bit native
 *  endian encoded audio messages. The base component produces a similar message, the
 *  analyzer component doens't produce any messages itself.
 *
 *  Depending on the type of input and output component you use for your soundcard,
 *  the amount of internal buffering of sound blocks can vary. In general, it's
 *  probably safe to assume that due to this buffering, the delay from the analyzer
 *  to the echo cancelling component will be at least twice the sampling interval.
 *  Because of this, it may be helpful to introduce some extra delay before sending
 *  an audio message to the analyzer component. You can do this using a MIPAudioMixer
 *  component, initialized with \c useTimeInfo set to \c false, and by using the
 *  MIPAudioMixer::setExtraDelay function.
 */
class EMIPLIB_IMPORTEXPORT MIPSpeexEchoCanceller : public MIPComponent
{
public:
	MIPSpeexEchoCanceller();
	~MIPSpeexEchoCanceller();

	/** Initialize the echo cancellation component.
	 *  Initialize the echo cancellation component.
	 *  \param sampRate The sampling rate of the audio messages.
	 *  \param interval The amount of time in each audio message.
	 *  \param bufferLength The length of the filter used by the Speex routines. 	 
	 */
	bool init(int sampRate, MIPTime interval = MIPTime(0.020), MIPTime bufferLength = MIPTime(0.100));

	/** Destroys the component. */
	bool destroy();
	
	/** Returns the analyzing sub-component (see class description). */
	MIPComponent *getOutputAnalyzer()								{ return m_pOutputAnalyzer; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();
	
	class OutputAnalyzer : public MIPComponent
	{
	public:
		OutputAnalyzer(void *pState, int sampRate, int numFrames) : MIPComponent("MIPSpeexEchoCanceller::OutputAnalyzer")
		{
			m_pSpeexEchoState = pState;
			m_sampRate = sampRate;
			m_numFrames = numFrames;
		}

		bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
		bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	private:
		void *m_pSpeexEchoState;
		int m_sampRate;
		int m_numFrames;
	};

	OutputAnalyzer *m_pOutputAnalyzer;
	void *m_pSpeexEchoState;
	int m_sampRate;
	int m_numFrames;

	std::list<MIPRaw16bitAudioMessage *> m_messages;
	std::list<MIPRaw16bitAudioMessage *>::const_iterator m_msgIt;
	int64_t m_prevIteration;
};

#endif // MIPCONFIG_SUPPORT_SPEEX

#endif // MIPSPEEXECHOCANCELLER_H

