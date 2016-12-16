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

#include "mipopusencoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include <opus/opus.h>

#include <iostream>

#include "mipdebug.h"

#define MIPOPUSENCODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPOPUSENCODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPOPUSENCODER_ERRSTR_BADINTERVAL			"The specified interval must correspond to 2.5ms, 5ms, 10ms, 20ms, 40ms or 60ms"
#define MIPOPUSENCODER_ERRSTR_BADMESSAGE			"Incoming messages must be either raw floating point audio messages or raw 16 bit signed audio messages"
#define MIPOPUSENCODER_ERRSTR_CANTCREATEENCODER			"Unable to create the encoder state"
#define MIPOPUSENCODER_ERRSTR_CANTSETBITRATE			"Unable to set the requested bitrate"
#define MIPOPUSENCODER_ERRSTR_ENCODERERROR			"Can't encode the incoming frame"
#define MIPOPUSENCODER_ERRSTR_ENCODERTYPE			"Unknown encoder type specified"
#define MIPOPUSENCODER_ERRSTR_INCOMPATIBLECHANNELS		"Number of channels of incoming message differs from the amount specified during initialization"
#define MIPOPUSENCODER_ERRSTR_INCOMPATIBLEFRAMES		"Number of frames of incoming message does not correspond to the interval specified during initialization"
#define MIPOPUSENCODER_ERRSTR_INCOMPATIBLESAMPRATE		"Sampling rate of incoming message differs from the one specified during initialization"
#define MIPOPUSENCODER_ERRSTR_INTERNALERRORBADSAMPRATE		"Internal error: unexpected sampling rate"
#define MIPOPUSENCODER_ERRSTR_INVALIDBITRATE			"The bitrate must lie between 6000 and 510000"
#define MIPOPUSENCODER_ERRSTR_INVALIDCHANNELS			"The number of channels must be either 1 or 2"
#define MIPOPUSENCODER_ERRSTR_INVALIDSAMPLINGRATE		"The sampling rate must be 8000, 12000, 16000, 24000 or 48000"

MIPOpusEncoder::MIPOpusEncoder() : MIPOutputMessageQueue("MIPOpusEncoder")
{
	m_init = false;
}

MIPOpusEncoder::~MIPOpusEncoder()
{
	destroy();
}

bool MIPOpusEncoder::init(int inputSamplingRate, int channels, EncoderType application, MIPTime interval, int targetBitrate)
{
	if (m_init)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_ALREADYINIT);
		return false;
	}

	if (inputSamplingRate != 8000 && inputSamplingRate != 12000 && inputSamplingRate != 16000 &&
	    inputSamplingRate != 24000 && inputSamplingRate != 48000)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_INVALIDSAMPLINGRATE);
		return false;
	}

	if (channels != 1 && channels != 2)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_INVALIDCHANNELS);
		return false;
	}

	int app = 0;

	switch(application)
	{
	case VoIP:
		app = OPUS_APPLICATION_VOIP;
		break;
	case Audio:
		app = OPUS_APPLICATION_AUDIO;
		break;
	case RestrictedLowDelay:
		app = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
		break;
	default:
		setErrorString(MIPOPUSENCODER_ERRSTR_ENCODERTYPE);
		return false;
	}

	m_inputFrames = (int)((real_t)inputSamplingRate * interval.getValue() + 0.5);

	switch(inputSamplingRate)
	{
	case 8000:
		if (m_inputFrames != 20 && m_inputFrames != 40 && m_inputFrames != 80 && m_inputFrames != 160
		    && m_inputFrames != 320 && m_inputFrames != 480)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_BADINTERVAL);
			return false;
		}
		break;
	case 12000:
		if (m_inputFrames != 30 && m_inputFrames != 60 && m_inputFrames != 120 && m_inputFrames != 240
		    && m_inputFrames != 480 && m_inputFrames != 720)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_BADINTERVAL);
			return false;
		}
		break;
	case 16000:
		if (m_inputFrames != 40 && m_inputFrames != 80 && m_inputFrames != 160 && m_inputFrames != 320
		    && m_inputFrames != 640 && m_inputFrames != 960)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_BADINTERVAL);
			return false;
		}
		break;
	case 24000:
		if (m_inputFrames != 60 && m_inputFrames != 120 && m_inputFrames != 240 && m_inputFrames != 480
		    && m_inputFrames != 960 && m_inputFrames != 1440)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_BADINTERVAL);
			return false;
		}
		break;
	case 48000:
		if (m_inputFrames != 120 && m_inputFrames != 240 && m_inputFrames != 480 && m_inputFrames != 960
		    && m_inputFrames != 1920 && m_inputFrames != 2880)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_BADINTERVAL);
			return false;
		}
		break;
	default:
		setErrorString(MIPOPUSENCODER_ERRSTR_INTERNALERRORBADSAMPRATE);
		return false;
	};

	if (targetBitrate != 0) // 0 specifies default bitrate
	{
		if (targetBitrate < 6000 || targetBitrate > 510000)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INVALIDBITRATE);
			return false;
		}
	}

	int error = 0;
	OpusEncoder *pEncoder = opus_encoder_create(inputSamplingRate, channels, app, &error);

	if (error != OPUS_OK)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_CANTCREATEENCODER);
		return false;
	}

	if (targetBitrate != 0)
	{
		if (opus_encoder_ctl(pEncoder, OPUS_SET_BITRATE(targetBitrate)) != 0)
		{
			opus_encoder_destroy(pEncoder);
			setErrorString(MIPOPUSENCODER_ERRSTR_CANTSETBITRATE);
			return false;
		}
	}

	MIPOutputMessageQueue::init();
	
	m_init = true;
	m_inputSamplingRate = inputSamplingRate;
	m_inputChannels = channels;
	m_pState = pEncoder;
	m_bufLength = 48000/100*12*2; // should be more than enough

	m_pBuffer = new uint8_t[m_bufLength];
	
	return true;
}

bool MIPOpusEncoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_NOTINIT);
		return false;
	}

	clearMessages();

	delete [] m_pBuffer;
	OpusEncoder *pEncoder = (OpusEncoder *)m_pState;
	opus_encoder_destroy(pEncoder);

	m_init = false;

	return true;
}

bool MIPOpusEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_NOTINIT);
		return false;
	}

	checkIteration(iteration);

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT || pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16) ) )
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_BADMESSAGE);
		return false;
	}

	OpusEncoder *pEncoder = (OpusEncoder *)m_pState;
	MIPEncodedAudioMessage *pNewMsg = 0;
	MIPMediaMessage *pInputMessage = (MIPMediaMessage *)pMsg;
	int numBytes = 0;

	if (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT)
	{
		MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;
	
		if (pAudioMsg->getSamplingRate() != m_inputSamplingRate)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INCOMPATIBLESAMPRATE);
			return false;
		}
		
		if (pAudioMsg->getNumberOfChannels() != m_inputChannels)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INCOMPATIBLECHANNELS);
			return false;
		}
		
		if (pAudioMsg->getNumberOfFrames() != m_inputFrames)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INCOMPATIBLEFRAMES);
			return false;
		}
	
		const float *pFloatBuf = pAudioMsg->getFrames();
		numBytes = opus_encode_float(pEncoder, pFloatBuf, m_inputFrames, m_pBuffer, m_bufLength);
	}
	else // Signed 16 bit, native endian encoded samples
	{
		MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;
	
		if (pAudioMsg->getSamplingRate() != m_inputSamplingRate)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INCOMPATIBLESAMPRATE);
			return false;
		}
		
		if (pAudioMsg->getNumberOfChannels() != m_inputChannels)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INCOMPATIBLECHANNELS);
			return false;
		}
		
		if (pAudioMsg->getNumberOfFrames() != m_inputFrames)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INCOMPATIBLEFRAMES);
			return false;
		}
	
		int16_t *pIntBuf = (int16_t *)pAudioMsg->getFrames();
		numBytes = opus_encode(pEncoder, pIntBuf, m_inputFrames, m_pBuffer, m_bufLength);
	}

	if (numBytes < 0)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_ENCODERERROR);
		return false;
	}

	uint8_t *pNewBuffer = new uint8_t[numBytes];
	memcpy(pNewBuffer, m_pBuffer, numBytes);
	
	pNewMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_OPUS, m_inputSamplingRate, m_inputChannels, m_inputFrames, pNewBuffer, numBytes, true);

	pNewMsg->copyMediaInfoFrom(*pInputMessage); // copy time and sourceID
	MIPOutputMessageQueue::addToOutputQueue(pNewMsg, true);

	return true;
}

bool MIPOpusEncoder::setBitrate(int targetBitrate)
{
	if (!m_init)
	{
		setErrorString(MIPOPUSENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (targetBitrate != 0) // 0 specifies default bitrate
	{
		if (targetBitrate < 6000 || targetBitrate > 510000)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_INVALIDBITRATE);
			return false;
		}
	}

	OpusEncoder *pEncoder = (OpusEncoder *)m_pState;

	if (targetBitrate != 0)
	{
		if (opus_encoder_ctl(pEncoder, OPUS_SET_BITRATE(targetBitrate)) != 0)
		{
			setErrorString(MIPOPUSENCODER_ERRSTR_CANTSETBITRATE);
			return false;
		}
	}

	return true;
}

#endif // MIPCONFIG_SUPPORT_OPUS

