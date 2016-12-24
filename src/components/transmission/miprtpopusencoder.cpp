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

#ifdef MIPCONFIG_SUPPORT_OPUS

#include "miprtpopusencoder.h"
#include "miprtpmessage.h"
#include "miprawaudiomessage.h"
#include "mipencodedaudiomessage.h"

#include "mipdebug.h"

#define MIPRTPOPUSENCODER_ERRSTR_BADMESSAGE		"Can't understand message"
#define MIPRTPOPUSENCODER_ERRSTR_NOTINIT		"RTP speex encoder not initialized"
#define MIPRTPOPUSENCODER_ERRSTR_BADSAMPLINGRATE	"Only sampling rates of 8, 12, 16, 24 and 48kHz are supported"

MIPRTPOpusEncoder::MIPRTPOpusEncoder() : MIPRTPEncoder("MIPRTPOpusEncoder")
{
	m_init = false;
}

MIPRTPOpusEncoder::~MIPRTPOpusEncoder()
{
	cleanUp();
}

bool MIPRTPOpusEncoder::init()
{
	if (m_init)
		cleanUp();

	MIPOutputMessageQueue::init();

	m_init = true;
	return true;
}

bool MIPRTPOpusEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPOPUSENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	checkIteration(iteration);
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_OPUS))
	{
		setErrorString(MIPRTPOPUSENCODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	MIPEncodedAudioMessage *pEncMsg = (MIPEncodedAudioMessage *)pMsg;
	int sampRate = pEncMsg->getSamplingRate();

	if (!(sampRate == 8000 || sampRate == 12000 || sampRate == 16000 || sampRate == 24000 || sampRate == 48000))
	{
		setErrorString(MIPRTPOPUSENCODER_ERRSTR_BADSAMPLINGRATE);
		return false;
	}
	
	const void *pData = pEncMsg->getData();
	bool marker = false; // Not needed
	size_t length = pEncMsg->getDataLength();
	uint8_t *pPayload = new uint8_t [length];
	int numFrames = pEncMsg->getNumberOfFrames();

	int tsInc = (48000/sampRate)*numFrames; // Opus uses an timestamp increment of 48000 each second

	memcpy(pPayload,pData,length);

	MIPRTPSendMessage *pNewMsg = new MIPRTPSendMessage(pPayload, length, getPayloadType(), marker, tsInc);
	pNewMsg->setSamplingInstant(pEncMsg->getTime());
	
	addToOutputQueue(pNewMsg, true);
		
	return true;
}

void MIPRTPOpusEncoder::cleanUp()
{
	clearMessages();
	m_init = false;
}

#endif // MIPCONFIG_SUPPORT_OPUS
