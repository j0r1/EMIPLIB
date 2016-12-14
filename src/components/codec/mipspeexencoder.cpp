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

#include "mipspeexencoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include <speex/speex.h>

#include "mipdebug.h"

#define MIPSPEEXENCODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPSPEEXENCODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPSPEEXENCODER_ERRSTR_NOTMONO				"Only mono audio is supported"
#define MIPSPEEXENCODER_ERRSTR_BADSAMPRATE			"Incompatible sampling rate"
#define MIPSPEEXENCODER_ERRSTR_BADFRAMES			"Incompatible number of frames"
#define MIPSPEEXENCODER_ERRSTR_BADMESSAGE			"Only floating point raw audio messages are accepted"
#define	MIPSPEEXENCODER_ERRSTR_BADQUALITYPARAM			"The quality parameter is invalid (valid range: 0..10)"
#define	MIPSPEEXENCODER_ERRSTR_BADCOMPLEXITYPARAM		"The complexity parameter is invalid (valid range: 1..10)"

MIPSpeexEncoder::MIPSpeexEncoder() : MIPComponent("MIPSpeexEncoder")
{
	m_init = false;
}

MIPSpeexEncoder::~MIPSpeexEncoder()
{
	destroy();
}

bool MIPSpeexEncoder::init(SpeexBandWidth b, int quality, int complexity, bool vad, bool dtx)
{
	if (m_init)
	{
		setErrorString(MIPSPEEXENCODER_ERRSTR_ALREADYINIT);
		return false;
	}

	if (quality < 0 || quality > 10)
	{
		setErrorString(MIPSPEEXENCODER_ERRSTR_BADQUALITYPARAM);
		return false;
	}

	if (complexity < 1 || complexity > 10)
	{
		setErrorString(MIPSPEEXENCODER_ERRSTR_BADCOMPLEXITYPARAM);
		return false;
	}

	m_pBits = new SpeexBits;
	speex_bits_init(m_pBits);

	if (b == NarrowBand)
	{
		m_pState = speex_encoder_init(&speex_nb_mode);
		m_sampRate = 8000;
	}
	else if (b == WideBand)
	{
		m_pState = speex_encoder_init(&speex_wb_mode);
		m_sampRate = 16000;
	}
	else
	{
		m_pState = speex_encoder_init(&speex_uwb_mode);
		m_sampRate = 32000;
	}
	speex_encoder_ctl(m_pState,SPEEX_GET_FRAME_SIZE,&m_numFrames);
	
	int useVad = (vad)?1:0;
	int useDtx = (dtx)?1:0;
	int q = quality;
	int c = complexity;
	speex_encoder_ctl(m_pState,SPEEX_SET_QUALITY,&q);
	speex_encoder_ctl(m_pState,SPEEX_SET_COMPLEXITY,&c);
	speex_encoder_ctl(m_pState,SPEEX_SET_VAD,&useVad);
	speex_encoder_ctl(m_pState,SPEEX_SET_DTX,&useDtx);
	
	m_pFloatBuffer = new float [m_numFrames];
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	
	m_init = true;
	
	return true;
}

bool MIPSpeexEncoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPSPEEXENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	speex_bits_destroy(m_pBits);
	delete m_pBits;
	speex_encoder_destroy(m_pState);
	clearMessages();
	delete [] m_pFloatBuffer;
	m_init = false;

	return true;
}

bool MIPSpeexEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSPEEXENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT || pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16) ) )
	{
		setErrorString(MIPSPEEXENCODER_ERRSTR_BADMESSAGE);
		return false;
	}

	if (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT)
	{
		MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;
	
		if (pAudioMsg->getSamplingRate() != m_sampRate)
		{
			setErrorString(MIPSPEEXENCODER_ERRSTR_BADSAMPRATE);
			return false;
		}
		
		if (pAudioMsg->getNumberOfChannels() != 1)
		{
			setErrorString(MIPSPEEXENCODER_ERRSTR_NOTMONO);
			return false;
		}
		
		if (pAudioMsg->getNumberOfFrames() != m_numFrames)
		{
			setErrorString(MIPSPEEXENCODER_ERRSTR_BADFRAMES);
			return false;
		}
	
		speex_bits_reset(m_pBits);
	
		const float *pFloatBuf = pAudioMsg->getFrames();
		for (int i = 0 ; i < m_numFrames ; i++)
			m_pFloatBuffer[i] = (float)(pFloatBuf[i]*32767.0);
		
		uint8_t *pBuffer = new uint8_t[m_numFrames]; // this should be more than enough memory
	
		speex_encode(m_pState, m_pFloatBuffer, m_pBits);
		size_t numBytes = (size_t)speex_bits_write(m_pBits, (char *)pBuffer, (int)m_numFrames);
		
		MIPEncodedAudioMessage *pNewMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX, m_sampRate, 1, m_numFrames, pBuffer, numBytes, true);
		pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
		m_messages.push_back(pNewMsg);
		
		m_msgIt = m_messages.begin();
	}
	else // Signed 16 bit, native endian encoded samples
	{
		MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
	
		if (pAudioMsg->getSamplingRate() != m_sampRate)
		{
			setErrorString(MIPSPEEXENCODER_ERRSTR_BADSAMPRATE);
			return false;
		}
		
		if (pAudioMsg->getNumberOfChannels() != 1)
		{
			setErrorString(MIPSPEEXENCODER_ERRSTR_NOTMONO);
			return false;
		}
		
		if (pAudioMsg->getNumberOfFrames() != m_numFrames)
		{
			setErrorString(MIPSPEEXENCODER_ERRSTR_BADFRAMES);
			return false;
		}
	
		speex_bits_reset(m_pBits);
	
		uint8_t *pBuffer = new uint8_t[m_numFrames]; // this should be more than enough memory
	
		speex_encode_int(m_pState, (int16_t *)pAudioMsg->getFrames(), m_pBits);
		size_t numBytes = (size_t)speex_bits_write(m_pBits, (char *)pBuffer, (int)m_numFrames);
		
		MIPEncodedAudioMessage *pNewMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX, m_sampRate, 1, m_numFrames, pBuffer, numBytes, true);
		pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
		m_messages.push_back(pNewMsg);
		
		m_msgIt = m_messages.begin();
	}
	
	return true;
}

bool MIPSpeexEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSPEEXENCODER_ERRSTR_NOTINIT);
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

void MIPSpeexEncoder::clearMessages()
{
	std::list<MIPEncodedAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

#endif // MIPCONFIG_SUPPORT_SPEEX

