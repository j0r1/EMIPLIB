/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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
#include "mipsampleencoder.h"

#define MIPSAMPLEENCODER_ERRSTR_BADTYPE				"Unsupported destination type"
#define MIPSAMPLEENCODER_ERRSTR_BADMESSAGETYPE			"Incoming message must be a raw audio message"
#define MIPSAMPLEENCODER_ERRSTR_INCOMPATIBLESUBTYPE		"Sample encoding is not the encoding set during initialization"
#define MIPSAMPLEENCODER_ERRSTR_NOTINIT				"Not initialized"

MIPSampleEncoder::MIPSampleEncoder() : MIPComponent("MIPSampleEncoder")
{
	m_init = false;
}

MIPSampleEncoder::~MIPSampleEncoder()
{
	cleanUp();
}

bool MIPSampleEncoder::init(int dstType)
{
	if (m_init)
		cleanUp();

	if (! (dstType == MIPRAWAUDIOMESSAGE_TYPE_FLOAT || dstType == MIPRAWAUDIOMESSAGE_TYPE_U16LE ||
	       dstType == MIPRAWAUDIOMESSAGE_TYPE_U16BE || dstType == MIPRAWAUDIOMESSAGE_TYPE_U8 ||
	       dstType == MIPRAWAUDIOMESSAGE_TYPE_S16BE || dstType == MIPRAWAUDIOMESSAGE_TYPE_S16LE || 
	       dstType == MIPRAWAUDIOMESSAGE_TYPE_U16 || dstType == MIPRAWAUDIOMESSAGE_TYPE_S16) )
	{
		setErrorString(MIPSAMPLEENCODER_ERRSTR_BADTYPE);
		return false;
	}

	m_dstType = dstType;
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_init = true;
	
	return true;
}

bool MIPSampleEncoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSAMPLEENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (pMsg->getMessageType() != MIPMESSAGE_TYPE_AUDIO_RAW)
	{
		setErrorString(MIPSAMPLEENCODER_ERRSTR_BADMESSAGETYPE);
		return false;
	}

	MIPAudioMessage *pAudioMsg = (MIPAudioMessage *)pMsg;

	size_t numIn = pAudioMsg->getNumberOfChannels()*pAudioMsg->getNumberOfFrames();

	float *pSamplesFloat = 0;
	uint8_t *pSamplesU8 = 0;
	uint16_t *pSamples16 = 0;

	if (m_dstType == MIPRAWAUDIOMESSAGE_TYPE_U8)
		pSamplesU8 = new uint8_t [numIn];
	else if (m_dstType == MIPRAWAUDIOMESSAGE_TYPE_FLOAT)
		pSamplesFloat = new float [numIn];
	else
		pSamples16 = new uint16_t [numIn];
	
	uint32_t srcType = pAudioMsg->getMessageSubtype();
	const float *pSamplesFloatIn = 0;
	const uint8_t *pSamplesU8In = 0;
	const uint16_t *pSamples16In = 0;
	
	if (srcType == MIPRAWAUDIOMESSAGE_TYPE_FLOAT)
	{
		MIPRawFloatAudioMessage *pMsg = (MIPRawFloatAudioMessage *)pAudioMsg;
		pSamplesFloatIn = pMsg->getFrames();
	}
	else if (srcType == MIPRAWAUDIOMESSAGE_TYPE_U8)
	{
		MIPRawU8AudioMessage *pMsg = (MIPRawU8AudioMessage *)pAudioMsg;
		pSamplesU8In = pMsg->getFrames();
	}
	else
	{
		MIPRaw16bitAudioMessage *pMsg = (MIPRaw16bitAudioMessage *)pAudioMsg;
		pSamples16In = pMsg->getFrames();
	}

	for (size_t i = 0 ; i < numIn ; i++)
	{
		uint8_t a;
		uint16_t b,c;
		int16_t d;
		int32_t value;

		switch(srcType)
		{
		case MIPRAWAUDIOMESSAGE_TYPE_FLOAT:
			value = ((int32_t)(pSamplesFloatIn[i]*32768.0f)) << 16;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U8:
			a = pSamplesU8In[i];
			value = (((int32_t)a)-128)<<24;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U16BE:
			b = pSamples16In[i];
#ifndef MIPCONFIG_BIGENDIAN // we're working on a little endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			value = (((int32_t)b)-32768) << 16;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U16LE:
			b = pSamples16In[i];
#ifdef MIPCONFIG_BIGENDIAN // we're working on a big endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			value = (((int32_t)b)-32768) << 16;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_S16BE:
			b = pSamples16In[i];
#ifndef MIPCONFIG_BIGENDIAN // we're working on a little endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			d = *((int16_t *)(&b));
			value = ((int32_t)d) << 16;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_S16LE:
			b = pSamples16In[i];
#ifdef MIPCONFIG_BIGENDIAN // we're working on a big endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			d = *((int16_t *)(&b));
			value = ((int32_t)d) << 16;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_S16:
			b = pSamples16In[i];
			d = *((int16_t *)(&b));
			value = ((int32_t)d) << 16;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U16:
			b = pSamples16In[i];
			value = (((int32_t)b)-32768) << 16;
			break;
		default:
			value = 0;
		}

		switch(m_dstType)
		{
		case MIPRAWAUDIOMESSAGE_TYPE_FLOAT:
			pSamplesFloat[i] = ((float)(value >> 16))/32768.0f;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U8:
			value = value >> 24;
			value += 128;
			a = (uint8_t)value;
			pSamplesU8[i] = a;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U16BE:
			value = value >> 16;
			value += 32768;
			b = (uint16_t)value;
#ifndef MIPCONFIG_BIGENDIAN // we're working on a little endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			pSamples16[i] = b;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U16LE:
			value = value >> 16;
			value += 32768;
			b = (uint16_t)value;
#ifdef MIPCONFIG_BIGENDIAN // we're working on a big endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			pSamples16[i] = b;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_S16BE:
			value = value >> 16;
			d = (int16_t)value;
			b = *((uint16_t *)&d);
#ifndef MIPCONFIG_BIGENDIAN // we're working on a little endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			pSamples16[i] = b;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_S16LE:
			value = value >> 16;
			d = (int16_t)value;
			b = *((uint16_t *)&d);
#ifdef MIPCONFIG_BIGENDIAN // we're working on a big endian system
			c = b;
			b = ((c >> 8) & 0xff) | ((c << 8) & 0xff00); // swap bytes
#endif // MIPCONFIG_BIGENDIAN
			pSamples16[i] = b;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_S16:
			value = value >> 16;
			d = (int16_t)value;
			b = *((uint16_t *)&d);
			pSamples16[i] = b;
			break;
		case MIPRAWAUDIOMESSAGE_TYPE_U16:
			value = value >> 16;
			value += 32768;
			b = (uint16_t)value;
			pSamples16[i] = b;
			break;
		default:
			// do nothing
			;
		}
	}

	MIPAudioMessage *pNewMsg;

	if (m_dstType == MIPRAWAUDIOMESSAGE_TYPE_U8)
		pNewMsg = new MIPRawU8AudioMessage(0,0,0,pSamplesU8,true);
	else if (m_dstType == MIPRAWAUDIOMESSAGE_TYPE_FLOAT)
		pNewMsg = new MIPRawFloatAudioMessage(0,0,0,pSamplesFloat,true);
	else
	{
		bool isSigned;
		MIPRaw16bitAudioMessage::SampleEncoding sampEnc;

		if (m_dstType == MIPRAWAUDIOMESSAGE_TYPE_S16LE || m_dstType == MIPRAWAUDIOMESSAGE_TYPE_S16BE || m_dstType == MIPRAWAUDIOMESSAGE_TYPE_S16)
			isSigned = true;
		else
			isSigned = false;
		
		if (m_dstType == MIPRAWAUDIOMESSAGE_TYPE_U16BE || m_dstType == MIPRAWAUDIOMESSAGE_TYPE_S16BE)
			sampEnc = MIPRaw16bitAudioMessage::BigEndian;
		else if (m_dstType == MIPRAWAUDIOMESSAGE_TYPE_U16LE || m_dstType == MIPRAWAUDIOMESSAGE_TYPE_S16LE)
			sampEnc = MIPRaw16bitAudioMessage::LittleEndian;
		else
			sampEnc = MIPRaw16bitAudioMessage::Native;
		
		pNewMsg = new MIPRaw16bitAudioMessage(0,0,0,isSigned,sampEnc,pSamples16,true);
	}

	pNewMsg->copyAudioInfoFrom(*pAudioMsg);

	if (m_prevIteration != iteration)
	{
		m_prevIteration = iteration;
		clearMessages();
	}

	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPSampleEncoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPSAMPLEENCODER_ERRSTR_NOTINIT);
		return false;
	}

	if (m_prevIteration != iteration)
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

void MIPSampleEncoder::cleanUp()
{
	if (!m_init)
		return;

	clearMessages();
	
	m_init = false;
}

void MIPSampleEncoder::clearMessages()
{
	std::list<MIPAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

