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

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "mipspeexechocanceller.h"
#include <speex/speex_echo.h>
#include <cmath>

#include "mipdebug.h"

#define MIPSPEEXECHOCANCELLER_ERRSTR_ALREADYINITIALIZED		"Already initialized"
#define MIPSPEEXECHOCANCELLER_ERRSTR_NOTINITIALIZED		"Component wasn't initialized"
#define MIPSPEEXECHOCANCELLER_ERRSTR_COULDNTCREATESTATE		"Couldn't create Speex echo state object"
#define MIPSPEEXECHOCANCELLER_ERRSTR_PULLNOTSUPPORTED		"This component doesn't support the 'pull' operation"
#define MIPSPEEXECHOCANCELLER_ERRSTR_BADMESSAGE			"Only 16 bit, native encoded raw audio messages are accepted"
#define MIPSPEEXECHOCANCELLER_ERRSTR_BADSAMPRATE		"Incoming message has a sampling rate different from the one used during initialization"
#define MIPSPEEXECHOCANCELLER_ERRSTR_NOTMONO			"Only mono audio messages are allowed"
#define MIPSPEEXECHOCANCELLER_ERRSTR_BADFRAMES			"The number of frames in the audio message doesn't correspond to the interval specified during initialization"

MIPSpeexEchoCanceller::MIPSpeexEchoCanceller() : MIPComponent("MIPSpeexEchoCanceller")
{
	m_pOutputAnalyzer = 0;
}

MIPSpeexEchoCanceller::~MIPSpeexEchoCanceller()
{
	destroy();
}

bool MIPSpeexEchoCanceller::init(int sampRate, MIPTime interval, MIPTime bufferLength)
{
	if (m_pOutputAnalyzer != 0)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_ALREADYINITIALIZED);
		return false;
	}

	m_sampRate = sampRate;
	m_numFrames = (int)((interval.getValue()*(real_t)sampRate)+0.5);
	
	int filterLength = (int)((bufferLength.getValue()*(real_t)sampRate)+0.5);
	int powerOfTwo = (int)((std::log((double)filterLength)/std::log(2.0))+0.5);
	
	if ((1 << powerOfTwo) < filterLength)
		powerOfTwo++;

	filterLength = 1 << powerOfTwo;

	m_pSpeexEchoState = speex_echo_state_init(m_numFrames, filterLength);
	if (m_pSpeexEchoState == 0)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_COULDNTCREATESTATE);
		return false;
	}

	m_pOutputAnalyzer = new OutputAnalyzer(m_pSpeexEchoState, m_sampRate, m_numFrames);
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPSpeexEchoCanceller::destroy()
{
	if (m_pOutputAnalyzer == 0)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	delete m_pOutputAnalyzer;
	speex_echo_state_destroy((SpeexEchoState *)m_pSpeexEchoState);
	clearMessages();

	m_pOutputAnalyzer = 0;
	
	return true;
}
	
bool MIPSpeexEchoCanceller::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pOutputAnalyzer == 0)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16) )
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;

	if (pAudioMsg->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_BADSAMPRATE);
		return false;
	}
	
	if (pAudioMsg->getNumberOfChannels() != 1)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_NOTMONO);
		return false;
	}
	
	if (pAudioMsg->getNumberOfFrames() != m_numFrames)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_BADFRAMES);
		return false;
	}
	
	uint16_t *pFrames = new uint16_t[m_numFrames];

	speex_echo_capture((SpeexEchoState *)m_pSpeexEchoState, (int16_t *)pAudioMsg->getFrames(), (int16_t *)pFrames);

	MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(m_sampRate, 1, m_numFrames, true, MIPRaw16bitAudioMessage::Native, pFrames, true);
	pNewMsg->copyMediaInfoFrom(*pAudioMsg);
	m_messages.push_back(pNewMsg);

	m_msgIt = m_messages.begin();

	return true;
}

bool MIPSpeexEchoCanceller::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pOutputAnalyzer == 0)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_NOTINITIALIZED);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (m_msgIt == m_messages.end())
	{
		*pMsg = 0;
		m_msgIt = m_messages.begin();
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	
	return true;
}

void MIPSpeexEchoCanceller::clearMessages()
{
	std::list<MIPRaw16bitAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}


bool MIPSpeexEchoCanceller::OutputAnalyzer::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16) )
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;

	if (pAudioMsg->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_BADSAMPRATE);
		return false;
	}
	
	if (pAudioMsg->getNumberOfChannels() != 1)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_NOTMONO);
		return false;
	}
	
	if (pAudioMsg->getNumberOfFrames() != m_numFrames)
	{
		setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_BADFRAMES);
		return false;
	}
	
	speex_echo_playback((SpeexEchoState *)m_pSpeexEchoState, (int16_t *)pAudioMsg->getFrames());
	
	return true;
}

bool MIPSpeexEchoCanceller::OutputAnalyzer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPSPEEXECHOCANCELLER_ERRSTR_PULLNOTSUPPORTED);
	return false;
}

#endif // MIPCONFIG_SUPPORT_SPEEX

