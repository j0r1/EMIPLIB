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

#include "mipsilkdecoder.h"
#include "mipencodedaudiomessage.h"
#include "miprawaudiomessage.h"
#include <SKP_Silk_SDK_API.h>

#include "mipdebug.h"

#define MIPSILKDECODER_ERRSTR_NOTINIT					"Not initialized"
#define MIPSILKDECODER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPSILKDECODER_ERRSTR_BADMESSAGE				"Bad message"
#define MIPSILKDECODER_ERRSTR_BADSAMPRATE				"The output sampling rate must be one of 8000, 12000, 16000, 24000, 32000, 44100 or 48000"
#define MIPSILKDECODER_ERRSTR_CANTGETSTATESIZE				"Unable to retrieve the required decoder state size"
#define MIPSILKDECODER_ERRSTR_CANTINITDECODER				"Can't initialize the decoder"
	
MIPSILKDecoder::MIPSILKDecoder() : MIPComponent("MIPSILKDecoder")
{
	m_init = false;
}

MIPSILKDecoder::~MIPSILKDecoder()
{
	destroy();
}

bool MIPSILKDecoder::init(int outputSamplingRate)
{
	if (m_init)
	{
		setErrorString(MIPSILKDECODER_ERRSTR_ALREADYINIT);
		return false;
	}
	
	if (!(outputSamplingRate == 8000 || outputSamplingRate == 12000 || outputSamplingRate == 16000 ||
	      outputSamplingRate == 24000 || outputSamplingRate == 32000 || outputSamplingRate == 44100 ||
	      outputSamplingRate == 48000 ))
	{
		setErrorString(MIPSILKDECODER_ERRSTR_BADSAMPRATE);
		return false;
	}

	m_outputSamplingRate = outputSamplingRate;

	m_lastIteration = -1;
	m_msgIt = m_messages.begin();
	m_lastExpireTime = MIPTime::getCurrentTime();
	m_init = true;
	return true;
}

bool MIPSILKDecoder::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPSILKDECODER_ERRSTR_NOTINIT);
		return false;
	}

	for (auto it = m_silkStates.begin() ; it != m_silkStates.end() ; it++)
		delete (*it).second;
	m_silkStates.clear();

	clearMessages();
	m_init = false;

	return true;
}

void MIPSILKDecoder::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPSILKDecoder::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();
	if ((curTime.getValue() - m_lastExpireTime.getValue()) < 60.0)
		return;

	auto it = m_silkStates.begin();
	while (it != m_silkStates.end())
	{
		if ((curTime.getValue() - (*it).second->getLastUpdateTime().getValue()) > 60.0)
		{
			auto it2 = it;
			it++;
	
			delete (*it2).second;
			m_silkStates.erase(it2);
		}
		else
			it++;
	}
	
	m_lastExpireTime = curTime;
}

bool MIPSILKDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSILKDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_ENCODED && pMsg->getMessageSubtype() == MIPENCODEDAUDIOMESSAGE_TYPE_SILK))
	{
		setErrorString(MIPSILKDECODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (iteration != m_lastIteration)
	{
		m_lastIteration = iteration;
		clearMessages();
		expire();
	}

	MIPEncodedAudioMessage *pEncMsg = (MIPEncodedAudioMessage *)pMsg;

	uint64_t sourceID = pEncMsg->getSourceID();

	auto it = m_silkStates.find(sourceID);
	SILKStateInfo *pInf = 0;

	if (it == m_silkStates.end()) // no entry present yet, add one
	{
		int32_t stateSize = 0;

		int status = SKP_Silk_SDK_Get_Decoder_Size(&stateSize);
		if (status != 0)
		{
			setErrorString(MIPSILKDECODER_ERRSTR_CANTGETSTATESIZE);
			return false;
		}

		uint8_t *pBuffer = new uint8_t[stateSize];

		status = SKP_Silk_SDK_InitDecoder(pBuffer);
		if (status != 0)
		{
			delete [] pBuffer;
			setErrorString(MIPSILKDECODER_ERRSTR_CANTINITDECODER);
			return false;
		}

		pInf = new SILKStateInfo(pBuffer);
		m_silkStates[sourceID] = pInf;
	}
	else
		pInf = (*it).second;

	pInf->setUpdateTime();

	const uint8_t *pData = pEncMsg->getData();
	int dataLength = pEncMsg->getDataLength();

	SKP_SILK_SDK_DecControlStruct decStruct;

	decStruct.API_sampleRate = m_outputSamplingRate;
	decStruct.frameSize = 0;
	decStruct.framesPerPacket = 1;
	decStruct.moreInternalDecoderFrames = 0;
	decStruct.inBandFECOffset = 0;

	int maxFrameSize = m_outputSamplingRate/10; // 100 ms
	uint16_t *pFrames = new uint16_t[maxFrameSize+maxFrameSize/5]; // allow room for one extra frame, in case the decoder acts strangely
	int16_t *pPtr = (int16_t *)pFrames;
	int offset = 0;
	bool done = false;
	int count = 0;

	while (!done)
	{
		if (offset >= maxFrameSize) // make sure we don't start overwriting stuff
			offset = 0;

		int16_t numSampOut = 0;
		int status = SKP_Silk_SDK_Decode(pInf->getDecoderState(), &decStruct, 0, pData, dataLength, &(pPtr[offset]), &numSampOut);
		if (status != 0) // bad packet, ignore it
		{
			delete [] pFrames;
			return true;
		}

		offset += numSampOut;
		count++;

		if (decStruct.moreInternalDecoderFrames == 0)
			done = true;
	}

	//std::cout << "Produced " << count << " frames" << std::endl;

	int numFrames = offset;

	// use 16 bit signed native encoding
	
	MIPRaw16bitAudioMessage *pNewMsg = new MIPRaw16bitAudioMessage(m_outputSamplingRate, 1, numFrames, true, MIPRaw16bitAudioMessage::Native, pFrames, true);
	pNewMsg->copyMediaInfoFrom(*pEncMsg); // copy source ID and message time
	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPSILKDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSILKDECODER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_lastIteration)
	{
		m_lastIteration = iteration;
		clearMessages();
		expire();
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

#endif // MIPCONFIG_SUPPORT_SILK

