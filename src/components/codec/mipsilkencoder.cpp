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

#ifdef MIPCONFIG_SUPPORT_SILK

#include "mipsilkencoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include <SKP_Silk_SDK_API.h>

#include <iostream>

#include "mipdebug.h"

#define MIPSILKENCODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPSILKENCODER_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPSILKENCODER_ERRSTR_CANTGETENCODERSIZE		"Unable to retrieve appropriate encoder state size"
//#define MIPSILKENCODER_ERRSTR_INVALIDBITRATE			"Bitrate must lie between 5000 and 100000 bps"
#define MIPSILKENCODER_ERRSTR_INVALIDENCODERSAMPLINGRATE	"Encoder sampling rate must be one of 8000, 12000, 16000 and 24000 Hz"
#define MIPSILKENCODER_ERRSTR_INVALIDINPUTSAMPLINGRATE		"Input sampling rate must be one of 8000, 12000, 16000, 24000, 32000, 44100 and 48000 Hz"
#define MIPSILKENCODER_ERRSTR_INVALIDINTERVAL			"Interval must be either 20, 40, 60, 80 or 100 ms"
#define MIPSILKENCODER_ERRSTR_BADMESSAGE			"Incoming message must be a mono raw audio one, using 16 bit native encoding"
#define MIPSILKENCODER_ERRSTR_NOTMONO				"Only mono input audio is supported"
#define MIPSILKENCODER_ERRSTR_INCOMPATIBLESAMPLINGRATE		"Sampling rate of incoming audio message is different from the one specified during initialization"
#define MIPSILKENCODER_ERRSTR_INCOMPATIBLEFRAMESIZE		"The number of samples in the incoming audio message does not correspond to the interval specified during initialization"
#define MIPSILKENCODER_ERRSTR_CANTINITENCODER			"Unable to initialize the SILK encoder"

MIPSILKEncoder::MIPSILKEncoder() : MIPComponent("MIPSILKEncoder")
{
	m_init = false;
}

MIPSILKEncoder::~MIPSILKEncoder()
{
	destroy();
}

bool MIPSILKEncoder::init(int samplingRate, MIPTime interval, int targetBitrate, int encoderSamplingRate)
{
	if (m_init)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_ALREADYINIT);
		return false;
	}

	if (!(samplingRate == 8000 || samplingRate == 12000 || samplingRate == 16000 || samplingRate == 24000 ||
              samplingRate == 32000 || samplingRate == 44100 || samplingRate == 48000 ))
	{
		setErrorString(MIPSILKENCODER_ERRSTR_INVALIDINPUTSAMPLINGRATE);
		return false;
	}

	if (!(encoderSamplingRate == 8000 || encoderSamplingRate == 12000 || encoderSamplingRate == 16000 ||
	      encoderSamplingRate == 24000 ))
	{
		setErrorString(MIPSILKENCODER_ERRSTR_INVALIDENCODERSAMPLINGRATE);
		return false;
	}

	if (targetBitrate < 0)
		targetBitrate = 0;

	/*
	if (targetBitrate < 5000 || targetBitrate > 100000)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_INVALIDBITRATE);
		return false;
	}
	*/

	int intervalMs = (int)(interval.getValue()*1000.0 + 0.5);

	if (!(intervalMs == 20 || intervalMs == 40 || intervalMs == 60 || intervalMs == 80 || intervalMs == 100 ))
	{
		setErrorString(MIPSILKENCODER_ERRSTR_INVALIDINTERVAL);
		return false;
	}

	int frameSize = (samplingRate*intervalMs)/1000;
	int frameSize2 = (int)((double)samplingRate * interval.getValue() + 0.5);

	if (frameSize2 != frameSize)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_INVALIDINTERVAL);
		return false;
	}

	int32_t encSize = 0;
	int status = SKP_Silk_SDK_Get_Encoder_Size(&encSize);

	if (status != 0)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_CANTGETENCODERSIZE);
		return false;
	}

	m_pState = new uint8_t[encSize];
	m_pEncoderControlStruct = new SKP_SILK_SDK_EncControlStruct;

	SKP_SILK_SDK_EncControlStruct *pCtrl = (SKP_SILK_SDK_EncControlStruct *)m_pEncoderControlStruct;

	status = SKP_Silk_SDK_InitEncoder(m_pState, pCtrl);
	if (status != 0)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_CANTINITENCODER);
		delete [] m_pState;
		delete pCtrl;
		return false;
	}

	pCtrl->API_sampleRate = samplingRate;
	pCtrl->maxInternalSampleRate = encoderSamplingRate;
	pCtrl->packetSize = frameSize;
	pCtrl->bitRate = targetBitrate;
	pCtrl->packetLossPercentage = 0;
	pCtrl->complexity = 2;
	pCtrl->useInBandFEC = 0;
	pCtrl->useDTX = 0; // TODO: when this is 1, output size can be 0!

	m_inputSamplingRate = samplingRate;
	m_maxInternalSamplingRate = samplingRate;
	m_frameSize = frameSize;
	m_targetBitrate = targetBitrate;

	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	
	m_init = true;
	
	return true;
}

bool MIPSILKEncoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_NOTINIT);
		return false;
	}
	
	SKP_SILK_SDK_EncControlStruct *pCtrl = (SKP_SILK_SDK_EncControlStruct *)m_pEncoderControlStruct;

	delete pCtrl;
	delete [] m_pState;

	clearMessages();
	m_init = false;

	return true;
}

bool MIPSILKEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 ) )
	{
		setErrorString(MIPSILKENCODER_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;

	if (pAudioMsg->getNumberOfChannels() != 1)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_NOTMONO);
		return false;
	}

	if (pAudioMsg->getSamplingRate() != m_inputSamplingRate)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}

	if (pAudioMsg->getNumberOfFrames() != m_frameSize)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_INCOMPATIBLEFRAMESIZE);
		return false;
	}

	const int16_t *pFrames = (const int16_t *)pAudioMsg->getFrames();
	int16_t outSize = m_frameSize*2;
	uint8_t *pOutData = new uint8_t[outSize]; // the input size should be more than adequate

	SKP_SILK_SDK_EncControlStruct *pCtrl = (SKP_SILK_SDK_EncControlStruct *)m_pEncoderControlStruct;

	int status = SKP_Silk_SDK_Encode(m_pState, pCtrl, pFrames, m_frameSize, pOutData, &outSize);
	if (status != 0)
	{
		char str[1024];
		sprintf(str, "Error using SILK encoder (error code %d)", status);
		delete [] pOutData;
		setErrorString(str);
		return false;
	}

	if (outSize == 0)
		std::cerr << "WARNING: MIPSILKEncoder::push -> outSize = 0!" << std::endl;

	// The number of frames is related to the interval by the input sampling rate
	MIPEncodedAudioMessage *pNewMsg = new MIPEncodedAudioMessage(MIPENCODEDAUDIOMESSAGE_TYPE_SILK, m_inputSamplingRate, 1, m_frameSize, pOutData, outSize, true);

	pNewMsg->copyMediaInfoFrom(*pAudioMsg); // copy time and sourceID
	m_messages.push_back(pNewMsg);
		
	m_msgIt = m_messages.begin();

	return true;
}

bool MIPSILKEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSILKENCODER_ERRSTR_NOTINIT);
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

void MIPSILKEncoder::clearMessages()
{
	std::list<MIPEncodedAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

#endif // MIPCONFIG_SUPPORT_SILK

