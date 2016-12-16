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

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_OPUS

#include "mipopusdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include <opus/opus.h>

#include "mipdebug.h"

#define MIPOPUSDECODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPOPUSDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPOPUSDECODER_ERRSTR_BADSAMPRATE				"Sampling rate must be either 8000, 12000, 16000, 24000 or 48000"
#define MIPOPUSDECODER_ERRSTR_BADCHANNELS				"Number of channels must be either 1 or 2"
#define MIPOPUSDECODER_ERRSTR_BADMESSAGE				"Bad message"
#define MIPOPUSDECODER_ERRSTR_CANCREATEDECODERSTATE			"Internal error: can't create Opus decoder state"

MIPOpusDecoder::OpusStateInfo::~OpusStateInfo()
{
	OpusDecoder *pState = (OpusDecoder *)m_pState;
	opus_decoder_destroy(pState);
}

MIPOpusDecoder::MIPOpusDecoder() : MIPOutputMessageQueue("MIPOpusDecoder")
{
	m_init = false;
}

MIPOpusDecoder::~MIPOpusDecoder()
{
	destroy();
}

bool MIPOpusDecoder::init(int outputSamplingRate, int channels, bool useFloat)
{
	if (m_init)
	{
		setErrorString(MIPOPUSDECODER_ERRSTR_ALREADYINIT);
		return false;
	}
	
	if (!(outputSamplingRate == 8000 || outputSamplingRate == 12000 || outputSamplingRate == 16000 ||
	      outputSamplingRate == 24000 || outputSamplingRate == 48000 ))
	{
		setErrorString(MIPOPUSDECODER_ERRSTR_BADSAMPRATE);
		return false;
	}

	if (channels != 1 && channels != 2)
	{
		setErrorString(MIPOPUSDECODER_ERRSTR_BADCHANNELS);
		return false;
	}

	m_outputSamplingRate = outputSamplingRate;
	m_outputChannels = channels;
	m_useFloat = useFloat;

	MIPOutputMessageQueue::init();
	MIPOutputMessageQueue::setExpirationDelay(60.0);
	m_init = true;
	return true;
}

bool MIPOpusDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPOPUSDECODER_ERRSTR_NOTINIT);
		return false;
	}

	MIPOutputMessageQueue::clearMessages();
	MIPOutputMessageQueue::clearStates();

	m_init = false;

	return true;
}

bool MIPOpusDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPOPUSDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_OPUS))
	{
		setErrorString(MIPOPUSDECODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	checkIteration(iteration);

	MIPEncodedAudioMessage *pEncMsg = (MIPEncodedAudioMessage *)pMsg;

	uint64_t sourceID = pEncMsg->getSourceID();

	OpusStateInfo *pStateInfo = (OpusStateInfo *)findState(sourceID);
	OpusDecoder *pDecoder = 0;

	if (pStateInfo == 0) 
	{
		int error = 0;
		pDecoder = opus_decoder_create(m_outputSamplingRate, m_outputChannels, &error);

		if (error != OPUS_OK)
		{
			setErrorString(MIPOPUSDECODER_ERRSTR_CANCREATEDECODERSTATE);
			return false; // shouldn't happen
		}

		pStateInfo = new OpusStateInfo(pDecoder);

		if (!MIPOutputMessageQueue::addState(sourceID, pStateInfo))
			return false; // shouldn't happen, error message already set
	}
	else
		pDecoder = (OpusDecoder *)pStateInfo->getState();

	pStateInfo->setUpdateTime();
	
	const uint8_t *pData = pEncMsg->getData();
	int dataLength = pEncMsg->getDataLength();

	int maxFrameSize = (m_outputSamplingRate/100)*12; // 120ms 
	MIPMediaMessage *pNewMsg = 0;
	
	if (!m_useFloat)
	{
		uint16_t *pFrames = new uint16_t[maxFrameSize*m_outputChannels];
		int16_t *pPtr = (int16_t *)pFrames;

		int numFrames = opus_decode(pDecoder, pData, dataLength, pPtr, maxFrameSize, 0);
		if (numFrames < 0)
		{
			// silently ignore decoding errors
			delete [] pFrames;
			return true; 
		}

		// use 16 bit signed native encoding
		
		pNewMsg = new MIPRaw16bitAudioMessage(m_outputSamplingRate, m_outputChannels, numFrames, true, MIPRaw16bitAudioMessage::Native, pFrames, true);
	}
	else
	{
		float *pFrames = new float[maxFrameSize*m_outputChannels];
		
		int numFrames = opus_decode_float(pDecoder, pData, dataLength, pFrames, maxFrameSize, 0);
		if (numFrames < 0)
		{
			// silently ignore decoding errors
			delete [] pFrames;
			return true; 
		}
		
		pNewMsg = new MIPRawFloatAudioMessage(m_outputSamplingRate, m_outputChannels, numFrames, pFrames, true);
	}

	pNewMsg->copyMediaInfoFrom(*pEncMsg); // copy source ID and message time

	MIPOutputMessageQueue::addToOutputQueue(pNewMsg, true);

	return true;
}

#endif // MIPCONFIG_SUPPORT_OPUS

