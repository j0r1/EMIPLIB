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

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SILK

#include "miprtpsilkencoder.h"
#include "miprtpmessage.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"

#include "mipdebug.h"

#define MIPRTPSILKENCODER_ERRSTR_BADMESSAGE		"Can't understand message"
#define MIPRTPSILKENCODER_ERRSTR_NOTINIT		"RTP SILK encoder not initialized"
#define MIPRTPSILKENCODER_ERRSTR_BADTIMESTAMPUNIT	"The timestamp must be increased each second by 8000, 12000, 16000 or 24000"
#define MIPRTPSILKENCODER_ERRSTR_BADSAMPLINGRATE	"Only sampling rates of 8, 12, 16, 24, 32, 44.1 and 48kHz are supported"

MIPRTPSILKEncoder::MIPRTPSILKEncoder() : MIPRTPEncoder("MIPRTPSILKEncoder")
{
	m_init = false;
}

MIPRTPSILKEncoder::~MIPRTPSILKEncoder()
{
	cleanUp();
}

bool MIPRTPSILKEncoder::init(int timestampPerSecond)
{
	if (!(timestampPerSecond == 8000 || timestampPerSecond == 12000 || timestampPerSecond == 16000 ||
	      timestampPerSecond == 24000) )
	{
		setErrorString(MIPRTPSILKENCODER_ERRSTR_BADTIMESTAMPUNIT);
		return false;
	}

	if (m_init)
		cleanUp();

	m_timestampsPerSecond = timestampPerSecond;

	m_prevIteration = -1;
	m_init = true;
	return true;
}

bool MIPRTPSILKEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPSILKENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_SILK))
	{
		setErrorString(MIPRTPSILKENCODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	MIPEncodedAudioMessage *pEncMsg = (MIPEncodedAudioMessage *)pMsg;
	int sampRate = pEncMsg->getSamplingRate();

	if (!(sampRate == 8000 || sampRate == 12000 || sampRate == 16000 || sampRate == 24000 || sampRate == 32000 ||
	      sampRate == 44100 || sampRate == 48000))
	{
		setErrorString(MIPRTPSILKENCODER_ERRSTR_BADSAMPLINGRATE);
		return false;
	}

	int numFrames = pEncMsg->getNumberOfFrames();
	int tsInc = (numFrames* m_timestampsPerSecond)/sampRate;
	
	const void *pData = pEncMsg->getData();
	bool marker = false;
	size_t length = pEncMsg->getDataLength();
	uint8_t *pPayload = new uint8_t [length];

	memcpy(pPayload, pData, length);

	MIPRTPSendMessage *pNewMsg;

	pNewMsg = new MIPRTPSendMessage(pPayload, length, getPayloadType(), marker, tsInc);
	pNewMsg->setSamplingInstant(pEncMsg->getTime());
	
	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
		
	return true;
}

bool MIPRTPSILKEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPSILKENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (m_msgIt == m_messages.end())
	{
		m_msgIt = m_messages.begin();
		*pMsg = 0;
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	return true;
}

void MIPRTPSILKEncoder::cleanUp()
{
	clearMessages();
	m_init = false;
}

void MIPRTPSILKEncoder::clearMessages()
{
	std::list<MIPRTPSendMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

#endif // MIPCONFIG_SUPPORT_SILK

