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

#if (defined(MIPCONFIG_SUPPORT_WINMM) || defined(MIPCONFIG_SUPPORT_OSS) || defined(MIPCONFIG_SUPPORT_PORTAUDIO) )

#include "mipaudiosession.h"
#ifdef MIPCONFIG_SUPPORT_WINMM
	#include "mipwinmminput.h"
	#include "mipwinmmoutput.h"
#else
	#include "mipossinputoutput.h"
	#include "mippainputoutput.h"
#endif // MIPCONFIG_SUPPORT_WINMM
#include "mipaudiosplitter.h"
#include "mipsampleencoder.h"
#include "mipspeexencoder.h"
#include "miplpcencoder.h"
#include "mipgsmencoder.h"
#include "mipulawencoder.h"
#include "mipalawencoder.h"
#include "miprtpspeexencoder.h"
#include "miprtplpcencoder.h"
#include "miprtpgsmencoder.h"
#include "miprtpalawencoder.h"
#include "miprtpulawencoder.h"
#include "miprtpl16encoder.h"
#include "miprtpcomponent.h"
#include "mipaveragetimer.h"
#include "mipinterchaintimer.h"
#include "miprtpdecoder.h"
#include "miprtpdummydecoder.h"
#include "miprtpspeexdecoder.h"
#include "miprtplpcdecoder.h"
#include "miprtpgsmdecoder.h"
#include "miprtpalawdecoder.h"
#include "miprtpulawdecoder.h"
#include "miprtpl16decoder.h"
#include "mipmediabuffer.h"
#include "mipspeexdecoder.h"
#include "miplpcdecoder.h"
#include "mipgsmdecoder.h"
#include "mipulawdecoder.h"
#include "mipalawdecoder.h"
#include "mipsamplingrateconverter.h"
#include "mipaudiomixer.h"
#include "mipencodedaudiomessage.h"
#include "mipmessagedumper.h"
#include "miprtpopusdecoder.h"
#include "miprtpopusencoder.h"
#include "mipopusdecoder.h"
#include "mipopusencoder.h"
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpudpv4transmitter.h>

//#include <iostream>

#include "mipdebug.h"

using namespace jrtplib;

#define MIPAUDIOSESSION_ERRSTR_NOTINIT						"Not initialized"
#define MIPAUDIOSESSION_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPAUDIOSESSION_ERRSTR_MULTIPLIERTOOSMALL				"The input and output sampling interval multipliers must be at least 1"
#define MIPAUDIOSESSION_ERRSTR_MULTIPLIERTOOLARGE				"The input and output sampling interval multipliers may not exceed 100"
#define MIPAUDIOSESSION_ERRSTR_NOSPEEX						"Can't use Speex codec since no Speex support was compiled in"
#define MIPAUDIOSESSION_ERRSTR_NOLPC						"Can't use LPC codec since no LPC support was compiled in"
#define MIPAUDIOSESSION_ERRSTR_NOGSM						"Can't use GSM codec since no GSM support was compiled in"
#define MIPAUDIOSESSION_ERRSTR_NOOPUS						"Can't use Opus codec since no Opus support was compiled in"
#define MIPAUDIOSESSION_ERRSTR_EQUALPAYLOADTYPES				"Incoming payload types for Speex and Opus must be different"

MIPAudioSession::MIPAudioSession()
{
	zeroAll();
	m_init = false;
}

MIPAudioSession::~MIPAudioSession()
{
	deleteAll();
}

bool MIPAudioSession::init(const MIPAudioSessionParams *pParams, MIPRTPSynchronizer *pSync, RTPSession *pRTPSession)
{
	if (m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_ALREADYINIT);
		return false;
	}

	MIPAudioSessionParams defaultParams;
	const MIPAudioSessionParams *pParams2 = &defaultParams;

	if (pParams)
		pParams2 = pParams;

	if (pParams2->getInputMultiplier() < 1 || pParams2->getOutputMultiplier() < 1)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_MULTIPLIERTOOSMALL);
		return false;
	}

	if (pParams2->getInputMultiplier() > 100 || pParams2->getOutputMultiplier() > 100)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_MULTIPLIERTOOLARGE);
		return false;
	}

	MIPTime inputInterval(0.020 * pParams2->getInputMultiplier());
	MIPTime outputInterval(0.020 * pParams2->getOutputMultiplier());

	int sampRate = 16000;
	int channels = 1;
	bool singleThread = false;

	if (pParams2->getCompressionType() == MIPAudioSessionParams::Speex)
	{
		if (pParams2->getSpeexEncoding() == MIPAudioSessionParams::NarrowBand)
			sampRate = 8000;
		else if (pParams2->getSpeexEncoding() == MIPAudioSessionParams::WideBand)
			sampRate = 16000;
		else // ultra wide band
			sampRate = 32000;
	}
	else if (pParams2->getCompressionType() == MIPAudioSessionParams::Opus)
		sampRate = 48000;
	else if (pParams2->getCompressionType() == MIPAudioSessionParams::L16Mono)
		sampRate = 44100;
	else
		sampRate = 8000;
		
	MIPComponentChain *pActiveChain = 0;
	MIPComponent *pPrevComponent = 0;
	MIPComponent *pInputComponent = 0;
	
#ifdef MIPCONFIG_SUPPORT_WINMM
	MIPWinMMInput *pInput = new MIPWinMMInput();
	storeComponent(pInput);
	
	if (!pInput->open(sampRate, channels, inputInterval, MIPTime(10.0), pParams2->getUseHighPriority(), pParams2->getInputDeviceID()))
	{
		setErrorString(pInput->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pInputChain = new InputChain(this);
	m_pInputChain->setChainStart(pInput);

	pActiveChain = m_pInputChain;
	pPrevComponent = pInput;
	pInputComponent = pInput;
#else

#ifndef MIPCONFIG_SUPPORT_PORTAUDIO
	MIPOSSInputOutput *pIOComp = 0;
	
	if (pParams2->getInputDeviceName() == pParams2->getOutputDeviceName())
	{
		MIPOSSInputOutputParams ioParams;
		
		MIPOSSInputOutput *pInput = new MIPOSSInputOutput();
		storeComponent(pInput);
		
		pIOComp = pInput;
		singleThread = true;

		ioParams.setDeviceName(pParams2->getInputDeviceName());

		if (!pInput->open(sampRate, channels, inputInterval, MIPOSSInputOutput::ReadWrite, &ioParams))
		{
			setErrorString(pInput->getErrorString());
			deleteAll();
			return false;
		}
		
		m_pIOChain = new IOChain(this);
		m_pIOChain->setChainStart(pInput);

		pActiveChain = m_pIOChain;
		pPrevComponent = pInput;
		pInputComponent = pInput;
	}
	else
	{
		MIPOSSInputOutputParams ioParams;
		
		MIPOSSInputOutput *pInput = new MIPOSSInputOutput();
		storeComponent(pInput);
		
		ioParams.setDeviceName(pParams2->getInputDeviceName());
		if (!pInput->open(sampRate, channels, inputInterval, MIPOSSInputOutput::ReadOnly, &ioParams))
		{
			setErrorString(pInput->getErrorString());
			deleteAll();
			return false;
		}
		
		m_pInputChain = new InputChain(this);
		m_pInputChain->setChainStart(pInput);

		pActiveChain = m_pInputChain;
		pPrevComponent = pInput;
		pInputComponent = pInput;
	}
#else // use PortAudio
	
	MIPPAInputOutput *pInput = new MIPPAInputOutput();
	MIPPAInputOutput *pIOComp = pInput;

	pInputComponent = pInput;

	storeComponent(pInput);
	singleThread = true;

	if (!pInput->open(sampRate, channels, inputInterval, MIPPAInputOutput::ReadWrite))
	{
		setErrorString(pInput->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pIOChain = new IOChain(this);
	m_pIOChain->setChainStart(pInput);

	pActiveChain = m_pIOChain;
	pPrevComponent = pInput;

#endif // !MIPCONFIG_SUPPORT_PORTAUDIO
	
#endif // MIPCONFIG_SUPPORT_WINMM

	if (pParams2->getInputMultiplier() > 1)
	{
		MIPAudioSplitter *pSplitter = new MIPAudioSplitter();
		storeComponent(pSplitter);
		
		if (!pSplitter->init(MIPTime(0.020)))
		{
			setErrorString(pSplitter->getErrorString());
			deleteAll();
			return false;
		}
		addLink(pActiveChain, &pPrevComponent, pSplitter);
	}

	MIPSampleEncoder *pSampEnc = new MIPSampleEncoder();
	storeComponent(pSampEnc);
	
	if (pParams2->getCompressionType() == MIPAudioSessionParams::L16Mono)
	{
		if (!pSampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16BE))
		{
			setErrorString(pSampEnc->getErrorString());
			deleteAll();
			return false;
		}
	}
	else
	{
		if (!pSampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16))
		{
			setErrorString(pSampEnc->getErrorString());
			deleteAll();
			return false;
		}
	}
	addLink(pActiveChain, &pPrevComponent, pSampEnc);
	
	switch (pParams2->getCompressionType())
	{
		case MIPAudioSessionParams::Speex:
		{
#ifdef MIPCONFIG_SUPPORT_SPEEX
			MIPSpeexEncoder *pSpeexEnc = new MIPSpeexEncoder();
			storeComponent(pSpeexEnc);
	
			MIPSpeexEncoder::SpeexBandWidth encoding;

			switch(pParams2->getSpeexEncoding())
			{
				case MIPAudioSessionParams::NarrowBand:
					encoding = MIPSpeexEncoder::NarrowBand;
					break;
				case MIPAudioSessionParams::WideBand:
					encoding = MIPSpeexEncoder::WideBand;
					break;
				case MIPAudioSessionParams::UltraWideBand:
				default:
					encoding = MIPSpeexEncoder::UltraWideBand;
			}
			
			if (!pSpeexEnc->init(encoding))
			{
				setErrorString(pSpeexEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pSpeexEnc);

			MIPRTPSpeexEncoder *pRTPEnc = new MIPRTPSpeexEncoder();
			storeComponent(pRTPEnc);
		
			if (!pRTPEnc->init())
			{
				setErrorString(pRTPEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pRTPEnc);

			pRTPEnc->setPayloadType(pParams2->getSpeexOutgoingPayloadType());
#else
			setErrorString(MIPAUDIOSESSION_ERRSTR_NOSPEEX);
			deleteAll();
			return false;
#endif // MIPCONFIG_SUPPORT_SPEEX
		}
		break;
		case MIPAudioSessionParams::LPC:
		{
#ifdef MIPCONFIG_SUPPORT_LPC
			MIPLPCEncoder *pLPCEnc = new MIPLPCEncoder();
			storeComponent(pLPCEnc);
	
			if (!pLPCEnc->init())
			{
				setErrorString(pLPCEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pLPCEnc);

			MIPRTPLPCEncoder *pRTPEnc = new MIPRTPLPCEncoder();
			storeComponent(pRTPEnc);
		
			if (!pRTPEnc->init())
			{
				setErrorString(pRTPEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pRTPEnc);

			pRTPEnc->setPayloadType(7);
#else
			setErrorString(MIPAUDIOSESSION_ERRSTR_NOLPC);
			deleteAll();
			return false;
#endif // MIPCONFIG_SUPPORT_LPC
		}
		break;
		case MIPAudioSessionParams::GSM:
		{
#ifdef MIPCONFIG_SUPPORT_GSM
			MIPGSMEncoder *pGSMEnc = new MIPGSMEncoder();
			storeComponent(pGSMEnc);
	
			if (!pGSMEnc->init())
			{
				setErrorString(pGSMEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pGSMEnc);

			MIPRTPGSMEncoder *pRTPEnc = new MIPRTPGSMEncoder();
			storeComponent(pRTPEnc);
		
			if (!pRTPEnc->init())
			{
				setErrorString(pRTPEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pRTPEnc);

			pRTPEnc->setPayloadType(3);
#else
			setErrorString(MIPAUDIOSESSION_ERRSTR_NOGSM);
			deleteAll();
			return false;
#endif // MIPCONFIG_SUPPORT_GSM
		}
		break;
		case MIPAudioSessionParams::ALaw:
		{
			MIPALawEncoder *pALawEnc = new MIPALawEncoder();
			storeComponent(pALawEnc);
	
			if (!pALawEnc->init())
			{
				setErrorString(pALawEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pALawEnc);

			MIPRTPALawEncoder *pRTPEnc = new MIPRTPALawEncoder();
			storeComponent(pRTPEnc);
		
			if (!pRTPEnc->init())
			{
				setErrorString(pRTPEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pRTPEnc);

			pRTPEnc->setPayloadType(8);
		}
		break;
		case MIPAudioSessionParams::L16Mono:
		{
			MIPRTPL16Encoder *pRTPEnc = new MIPRTPL16Encoder();
			storeComponent(pRTPEnc);

			if (!pRTPEnc->init(false)) // initialize as mono
			{
				setErrorString(pRTPEnc->getErrorString());
				deleteAll();
				return false;
			}

			addLink(pActiveChain, &pPrevComponent, pRTPEnc);

			pRTPEnc->setPayloadType(11);
		}
		break;
		case MIPAudioSessionParams::Opus:
		{
#ifdef MIPCONFIG_SUPPORT_OPUS
			MIPOpusEncoder *pOpusEnc = new MIPOpusEncoder();
			storeComponent(pOpusEnc);
	
			if (!pOpusEnc->init(sampRate, 1, MIPOpusEncoder::VoIP, MIPTime(0.020), pParams2->getOpusBandwidth()))
			{
				setErrorString(pOpusEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pOpusEnc);

			MIPRTPOpusEncoder *pRTPEnc = new MIPRTPOpusEncoder();
			storeComponent(pRTPEnc);
		
			if (!pRTPEnc->init())
			{
				setErrorString(pRTPEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pRTPEnc);

			pRTPEnc->setPayloadType(pParams2->getOpusOutgoingPayloadType());
#else
			setErrorString(MIPAUDIOSESSION_ERRSTR_NOOPUS);
			deleteAll();
			return false;
#endif // MIPCONFIG_SUPPORT_OPUS
		}
		break;
		case MIPAudioSessionParams::ULaw:
		default:
		{
			MIPULawEncoder *pULawEnc = new MIPULawEncoder();
			storeComponent(pULawEnc);
	
			if (!pULawEnc->init())
			{
				setErrorString(pULawEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pULawEnc);

			MIPRTPULawEncoder *pRTPEnc = new MIPRTPULawEncoder();
			storeComponent(pRTPEnc);
		
			if (!pRTPEnc->init())
			{
				setErrorString(pRTPEnc->getErrorString());
				deleteAll();
				return false;
			}
			addLink(pActiveChain, &pPrevComponent, pRTPEnc);

			pRTPEnc->setPayloadType(0);
		}
	}

	if (pRTPSession != 0)
	{
		int status;
		
		m_pRTPSession = pRTPSession;
		m_deleteRTPSession = false;

		if ((status = m_pRTPSession->SetTimestampUnit(1.0/((double)sampRate))) < 0)
		{
			setErrorString(RTPGetErrorString(status));
			deleteAll();
			return false;		
		}
	}
	else
	{
		RTPUDPv4TransmissionParams transParams;
		RTPSessionParams sessParams;
		int status;
		
		transParams.SetPortbase(pParams2->getPortbase());
		sessParams.SetOwnTimestampUnit(1.0/((double)sampRate));
		sessParams.SetMaximumPacketSize(2000);
		sessParams.SetAcceptOwnPackets(pParams2->getAcceptOwnPackets());

		m_pRTPSession = new RTPSession();
		m_deleteRTPSession = true;
	
		if ((status = m_pRTPSession->Create(sessParams,&transParams)) < 0)
		{
			setErrorString(RTPGetErrorString(status));
			deleteAll();
			return false;
		}
	}

	m_pRTPComp = new MIPRTPComponent();
	storeComponent(m_pRTPComp);
	
	if (!m_pRTPComp->init(m_pRTPSession))
	{
		setErrorString(m_pRTPComp->getErrorString());
		deleteAll();
		return false;
	}
	addLink(pActiveChain, &pPrevComponent, m_pRTPComp);

	bool usingInterChainTiming = true;

	if (pParams2->getDisableInterChainTimer())
		usingInterChainTiming = false;
	else
	{
		int m = pParams2->getOutputMultiplier() % pParams2->getInputMultiplier();

		if (m != 0) // not possible to use inter-chain timing
			usingInterChainTiming = false;
	}

	if (!singleThread)
	{
		m_pOutputChain = new OutputChain(this);

		if (!usingInterChainTiming)
		{
			MIPAverageTimer *pTimer = new MIPAverageTimer(outputInterval);
			storeComponent(pTimer);
		
			m_pOutputChain->setChainStart(pTimer);
		
			pActiveChain = m_pOutputChain;
			pPrevComponent = pTimer;
			addLink(pActiveChain, &pPrevComponent, m_pRTPComp);
		}
		else
		{
			MIPInterChainTimer *pTimer = new MIPInterChainTimer();
			storeComponent(pTimer);
			
			int count = pParams2->getOutputMultiplier() / pParams2->getInputMultiplier();

			if (!pTimer->init(count, MIPTime(inputInterval.getValue()*10.0)))
			{
				setErrorString(pTimer->getErrorString());
				deleteAll();
				return false;
			}

			m_pInputChain->addConnection(pInputComponent, pTimer->getTriggerComponent());
			m_pOutputChain->setChainStart(pTimer);
		
			pActiveChain = m_pOutputChain;
			pPrevComponent = pTimer;

			addLink(pActiveChain, &pPrevComponent, m_pRTPComp);
		}
	}
	
	MIPRTPDecoder *pRTPDec = new MIPRTPDecoder();
	storeComponent(pRTPDec);
	
	if (!pRTPDec->init(true, pSync, m_pRTPSession))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}
	addLink(pActiveChain, &pPrevComponent, pRTPDec);

	MIPRTPDummyDecoder *pRTPDummyDec = new MIPRTPDummyDecoder();
	storePacketDecoder(pRTPDummyDec);
	
	if (!pRTPDec->setDefaultPacketDecoder(pRTPDummyDec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}	

#if defined(MIPCONFIG_SUPPORT_SPEEX) && defined(MIPCONFIG_SUPPORT_OPUS)
	if (pParams2->getSpeexIncomingPayloadType() == pParams2->getOpusIncomingPayloadType())
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_EQUALPAYLOADTYPES);
		deleteAll();
		return false;
	}
#endif // MIPCONFIG_SUPPORT_SPEEX && MIPCONFIG_SUPPORT_OPUS

#ifdef MIPCONFIG_SUPPORT_SPEEX
	MIPRTPSpeexDecoder *pRTPSpeexDec = new MIPRTPSpeexDecoder();
	storePacketDecoder(pRTPSpeexDec);
	
	if (!pRTPDec->setPacketDecoder(pParams2->getSpeexIncomingPayloadType(), pRTPSpeexDec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}
#endif // MIPCONFIG_SUPPORT_SPEEX

#ifdef MIPCONFIG_SUPPORT_LPC
	MIPRTPLPCDecoder *pRTPLPCDec = new MIPRTPLPCDecoder();
	storePacketDecoder(pRTPLPCDec);
	
	if (!pRTPDec->setPacketDecoder(7, pRTPLPCDec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}
#endif // MIPCONFIG_SUPPORT_LPC

#ifdef MIPCONFIG_SUPPORT_GSM
	MIPRTPGSMDecoder *pRTPGSMDec = new MIPRTPGSMDecoder();
	storePacketDecoder(pRTPGSMDec);
	
	if (!pRTPDec->setPacketDecoder(3, pRTPGSMDec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}
#endif // MIPCONFIG_SUPPORT_GSM

#ifdef MIPCONFIG_SUPPORT_OPUS
	MIPRTPOpusDecoder *pRTPOpusDec = new MIPRTPOpusDecoder();
	storePacketDecoder(pRTPOpusDec);
	
	if (!pRTPDec->setPacketDecoder(pParams2->getOpusIncomingPayloadType(), pRTPOpusDec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}
#endif // MIPCONFIG_SUPPORT_OPUS

	MIPRTPALawDecoder *pRTPALawDec = new MIPRTPALawDecoder();
	storePacketDecoder(pRTPALawDec);
	
	if (!pRTPDec->setPacketDecoder(8, pRTPALawDec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}

	MIPRTPULawDecoder *pRTPULawDec = new MIPRTPULawDecoder();
	storePacketDecoder(pRTPULawDec);
	
	if (!pRTPDec->setPacketDecoder(0, pRTPULawDec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}

	MIPRTPL16Decoder *pRTPL16Dec = new MIPRTPL16Decoder(false); // for mono sound
	storePacketDecoder(pRTPL16Dec);

	if (!pRTPDec->setPacketDecoder(11, pRTPL16Dec))
	{
		setErrorString(pRTPDec->getErrorString());
		deleteAll();
		return false;
	}

	MIPMediaBuffer *pMediaBuf = new MIPMediaBuffer();
	storeComponent(pMediaBuf);
	
	if (!pMediaBuf->init(outputInterval))
	{
		setErrorString(pMediaBuf->getErrorString());
		deleteAll();
		return false;
	}
	addLink(pActiveChain, &pPrevComponent, pMediaBuf, true);
	
	MIPSamplingRateConverter *pSampConv = new MIPSamplingRateConverter();
	storeComponent(pSampConv);
	
	if (!pSampConv->init(sampRate, channels, false))
	{
		setErrorString(pSampConv->getErrorString());
		deleteAll();
		return false;
	}
	
#ifdef MIPCONFIG_SUPPORT_SPEEX
	MIPSpeexDecoder *pSpeexDec = new MIPSpeexDecoder();
	storeComponent(pSpeexDec);
	
	if (!pSpeexDec->init(false))
	{
		setErrorString(pSpeexDec->getErrorString());
		deleteAll();
		return false;
	}
	pActiveChain->addConnection(pMediaBuf, pSpeexDec, false, MIPMESSAGE_TYPE_AUDIO_ENCODED, MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX);
	pActiveChain->addConnection(pSpeexDec, pSampConv, false);
#endif // MIPCONFIG_SUPPORT_SPEEX
	
#ifdef MIPCONFIG_SUPPORT_OPUS
	MIPOpusDecoder *pOpusDec = new MIPOpusDecoder();
	storeComponent(pOpusDec);
	
	if (!pOpusDec->init(sampRate, 1, false))
	{
		setErrorString(pOpusDec->getErrorString());
		deleteAll();
		return false;
	}
	pActiveChain->addConnection(pMediaBuf, pOpusDec, false, MIPMESSAGE_TYPE_AUDIO_ENCODED, MIPENCODEDAUDIOMESSAGE_TYPE_OPUS);
	pActiveChain->addConnection(pOpusDec, pSampConv, false);
#endif // MIPCONFIG_SUPPORT_OPUS

#ifdef MIPCONFIG_SUPPORT_LPC
	MIPLPCDecoder *pLPCDec = new MIPLPCDecoder();
	storeComponent(pLPCDec);
	
	if (!pLPCDec->init())
	{
		setErrorString(pLPCDec->getErrorString());
		deleteAll();
		return false;
	}
	pActiveChain->addConnection(pMediaBuf, pLPCDec, false, MIPMESSAGE_TYPE_AUDIO_ENCODED, MIPENCODEDAUDIOMESSAGE_TYPE_LPC);
	pActiveChain->addConnection(pLPCDec, pSampConv, false);
#endif // MIPCONFIG_SUPPORT_LPC

#ifdef MIPCONFIG_SUPPORT_GSM
	MIPGSMDecoder *pGSMDec = new MIPGSMDecoder();
	storeComponent(pGSMDec);
	
	if (!pGSMDec->init())
	{
		setErrorString(pGSMDec->getErrorString());
		deleteAll();
		return false;
	}
	pActiveChain->addConnection(pMediaBuf, pGSMDec, false, MIPMESSAGE_TYPE_AUDIO_ENCODED, MIPENCODEDAUDIOMESSAGE_TYPE_GSM);
	pActiveChain->addConnection(pGSMDec, pSampConv, false);
#endif // MIPCONFIG_SUPPORT_GSM

	MIPALawDecoder *pALawDec = new MIPALawDecoder();
	storeComponent(pALawDec);
	
	if (!pALawDec->init())
	{
		setErrorString(pALawDec->getErrorString());
		deleteAll();
		return false;
	}
	pActiveChain->addConnection(pMediaBuf, pALawDec, false, MIPMESSAGE_TYPE_AUDIO_ENCODED, MIPENCODEDAUDIOMESSAGE_TYPE_ALAW);
	pActiveChain->addConnection(pALawDec, pSampConv, false);

	MIPULawDecoder *pULawDec = new MIPULawDecoder();
	storeComponent(pULawDec);
	
	if (!pULawDec->init())
	{
		setErrorString(pULawDec->getErrorString());
		deleteAll();
		return false;
	}
	pActiveChain->addConnection(pMediaBuf, pULawDec, true, MIPMESSAGE_TYPE_AUDIO_ENCODED, MIPENCODEDAUDIOMESSAGE_TYPE_ULAW);
	pActiveChain->addConnection(pULawDec, pSampConv, true);

	MIPSampleEncoder *pL16SampDec = new MIPSampleEncoder();
	storeComponent(pL16SampDec);

	if (!pL16SampDec->init(MIPRAWAUDIOMESSAGE_TYPE_S16))
	{
		setErrorString(pL16SampDec->getErrorString());
		deleteAll();
		return false;
	}
	pActiveChain->addConnection(pMediaBuf, pL16SampDec, false, MIPMESSAGE_TYPE_AUDIO_RAW, MIPRAWAUDIOMESSAGE_TYPE_S16BE);
	pActiveChain->addConnection(pL16SampDec, pSampConv, false);


	pPrevComponent = pSampConv;
	
	MIPAudioMixer *pMixer = new MIPAudioMixer();
	storeComponent(pMixer);
	
	if (!pMixer->init(sampRate, channels, outputInterval, true, false))
	{
		setErrorString(pMixer->getErrorString());
		deleteAll();
		return false;
	}
	addLink(pActiveChain, &pPrevComponent, pMixer, true);

	uint32_t destType;
#ifdef MIPCONFIG_SUPPORT_WINMM
	destType = MIPRAWAUDIOMESSAGE_TYPE_S16LE;
	
	MIPWinMMOutput *pOutput = new MIPWinMMOutput();
	storeComponent(pOutput);
	
	if (!pOutput->open(sampRate, channels, outputInterval, MIPTime(10.0), pParams2->getUseHighPriority(), pParams2->getOutputDeviceID()))
	{
		setErrorString(pOutput->getErrorString());
		deleteAll();
		return false;
	}
#else

#ifndef MIPCONFIG_SUPPORT_PORTAUDIO
	MIPOSSInputOutput *pOutput;
	
	if (singleThread)
		pOutput = pIOComp;
	else
	{
		pOutput = new MIPOSSInputOutput();
		storeComponent(pOutput);
		
		MIPOSSInputOutputParams ioParams;
		ioParams.setDeviceName(pParams2->getOutputDeviceName());
		
		if (!pOutput->open(sampRate, channels, outputInterval, MIPOSSInputOutput::WriteOnly, &ioParams))
		{
			setErrorString(pOutput->getErrorString());
			deleteAll();
			return false;
		}
	}

	destType = pOutput->getRawAudioSubtype();
#else
	MIPPAInputOutput *pOutput = pIOComp;

	destType = MIPRAWAUDIOMESSAGE_TYPE_S16;
#endif // !MIPCONFIG_SUPPORT_PORTAUDIO

#endif // WIN32 || _WIN32_WCE
	
	MIPSampleEncoder *pSampEnc2 = new MIPSampleEncoder();
	storeComponent(pSampEnc2);
	
	if (!pSampEnc2->init(destType))
	{
		setErrorString(pSampEnc2->getErrorString());
		deleteAll();
		return false;
	}
	addLink(pActiveChain, &pPrevComponent, pSampEnc2, true);
	addLink(pActiveChain, &pPrevComponent, pOutput, true);

	if (!singleThread)	
	{
		if (!m_pInputChain->start())
		{
			setErrorString(m_pInputChain->getErrorString());
			deleteAll();
			return false;
		}
	
		if (!m_pOutputChain->start())
		{
			setErrorString(m_pOutputChain->getErrorString());
			deleteAll();
			return false;
		}
	}
	else // single I/O thread
	{
		if (!m_pIOChain->start())
		{
			setErrorString(m_pIOChain->getErrorString());
			deleteAll();
			return false;
		}
	}
	
	m_init = true;
	return true;
}

bool MIPAudioSession::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	deleteAll();
	m_init = false;
	return true;
}

// Note: since we can't be sure that the underlying RTPTransmitter
// was compiled in a thread-safe way, we'll lock the RTP component

bool MIPAudioSession::addDestination(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;

	m_pRTPComp->lock();
	if ((status = m_pRTPSession->AddDestination(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();

	return true;
}

bool MIPAudioSession::deleteDestination(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;
	
	m_pRTPComp->lock();
	if ((status = m_pRTPSession->DeleteDestination(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();

	return true;
}

bool MIPAudioSession::clearDestinations()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->ClearDestinations();
	m_pRTPComp->unlock();
	return true;
}

bool MIPAudioSession::supportsMulticasting()
{
	if (!m_init)
		return false;
	
	m_pRTPComp->lock();
	bool val = m_pRTPSession->SupportsMulticasting();
	m_pRTPComp->unlock();

	return val;
}

bool MIPAudioSession::joinMulticastGroup(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;

	m_pRTPComp->lock();
	if ((status = m_pRTPSession->JoinMulticastGroup(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::leaveMulticastGroup(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;

	m_pRTPComp->lock();
	if ((status = m_pRTPSession->LeaveMulticastGroup(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::leaveAllMulticastGroups()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->LeaveAllMulticastGroups();
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::setReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;

	m_pRTPComp->lock();
	if ((status = m_pRTPSession->SetReceiveMode(m)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::addToIgnoreList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;

	m_pRTPComp->lock();
	if ((status = m_pRTPSession->AddToIgnoreList(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::deleteFromIgnoreList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;
	
	m_pRTPComp->lock();
	if ((status = m_pRTPSession->DeleteFromIgnoreList(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::clearIgnoreList()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->ClearIgnoreList();
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::addToAcceptList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;

	m_pRTPComp->lock();
	if ((status = m_pRTPSession->AddToAcceptList(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::deleteFromAcceptList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;

	m_pRTPComp->lock();
	if ((status = m_pRTPSession->DeleteFromAcceptList(addr)) < 0)
	{
		m_pRTPComp->unlock();
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	m_pRTPComp->unlock();
	
	return true;
}

bool MIPAudioSession::clearAcceptList()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->ClearAcceptList();
	m_pRTPComp->unlock();
	
	return true;
}

void MIPAudioSession::zeroAll()
{
	m_pInputChain = 0;
	m_pOutputChain = 0;
	m_pIOChain = 0;
	m_pRTPSession = 0;

	m_components.clear();
	m_packetDecoders.clear();
}

void MIPAudioSession::deleteAll()
{
	// Note: since the output chain may be driven by the input chain, it's a good idea to stop the output chain
	// first.

	if (m_pOutputChain)
	{
		m_pOutputChain->stop();
		delete m_pOutputChain;
	}	
	
	if (m_pInputChain)
	{
		m_pInputChain->stop();
		delete m_pInputChain;
	}

	if (m_pIOChain)
	{
		m_pIOChain->stop();
		delete m_pIOChain;
	}

	std::list<MIPComponent *>::const_iterator it;

	for (it = m_components.begin() ; it != m_components.end() ; it++)
	{
		//std::cout << "Deleting " << (*it)->getComponentName() << std::endl;
		delete (*it);
	}
	m_components.clear();
	
	std::list<MIPRTPPacketDecoder *>::const_iterator it2;

	for (it2 = m_packetDecoders.begin() ; it2 != m_packetDecoders.end() ; it2++)
		delete (*it2);
	m_packetDecoders.clear();

	// Note: this should be deleted last, after we can be certain that all RTP
	// packets from this session have been deleted
	if (m_pRTPSession)
	{
		if (m_deleteRTPSession)
		{
			m_pRTPSession->BYEDestroy(RTPTime(2,0),0,0);
			delete m_pRTPSession;
		}
	}

	zeroAll();
}

void MIPAudioSession::storeComponent(MIPComponent *pComp)
{
	bool found = false;
	std::list<MIPComponent *>::const_iterator it;
	
	for (it = m_components.begin() ; !found && it != m_components.end() ; it++)
		if (pComp == (*it))
			found = true;

	if (!found)
		m_components.push_back(pComp);
}

void MIPAudioSession::storePacketDecoder(MIPRTPPacketDecoder *pDec)
{
	bool found = false;
	std::list<MIPRTPPacketDecoder *>::const_iterator it;
	
	for (it = m_packetDecoders.begin() ; !found && it != m_packetDecoders.end() ; it++)
		if (pDec == (*it))
			found = true;

	if (!found)
		m_packetDecoders.push_back(pDec);
}

void MIPAudioSession::addLink(MIPComponentChain *pChain, MIPComponent **pPrevComp, MIPComponent *pComp, 
                              bool feedback, uint32_t mask1, uint32_t mask2)
{
	pChain->addConnection(*pPrevComp, pComp, feedback, mask1, mask2);
//	std::cout << "Adding link from " << (*pPrevComp)->getComponentName() << " to " << pComp->getComponentName() << std::endl;
	*pPrevComp = pComp;
}

#endif // MIPCONFIG_SUPPORT_WINMM || MIPCONFIG_SUPPORT_OSS || MIPCONFIG_SUPPORT_PORTAUDIO

