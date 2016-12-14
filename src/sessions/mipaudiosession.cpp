/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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

#include "mipaudiosession.h"
#if (defined(WIN32) || defined(_WIN32_WCE))
	#include "mipwinmminput.h"
	#include "mipwinmmoutput.h"
#else
	#include "mipossinputoutput.h"
#endif // WIN32 || _WIN32_WCE
#include "mipaudiosplitter.h"
#include "mipsampleencoder.h"
#include "mipspeexencoder.h"
#include "miprtpaudioencoder.h"
#include "miprtpcomponent.h"
#include "mipaveragetimer.h"
#include "miprtpaudiodecoder.h"
#include "mipmediabuffer.h"
#include "mipspeexdecoder.h"
#include "mipaudiomixer.h"
#include "mipencodedaudiomessage.h"
#include <rtpsession.h>
#include <rtpsessionparams.h>
#include <rtperrors.h>
#include <rtpudpv4transmitter.h>

#define MIPAUDIOSESSION_ERRSTR_NOTINIT						"Not initialized"
#define MIPAUDIOSESSION_ERRSTR_ALREADYINIT					"Already initialized"

MIPAudioSession::MIPAudioSession()
{
	zeroAll();
	m_init = false;
}

MIPAudioSession::~MIPAudioSession()
{
	deleteAll();
}

bool MIPAudioSession::init(const MIPAudioSessionParams *pParams, MIPRTPSynchronizer *pSync)
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

#ifdef _WIN32_WCE
	MIPTime inputInterval(0.060);
	MIPTime outputInterval(0.040);
#else
	MIPTime inputInterval(0.020);
	MIPTime outputInterval(0.020);
#endif // _WIN32_WCE
	int sampRate = 16000;
	int channels = 1;
	
#if (defined(WIN32) || defined(_WIN32_WCE))
	m_pInput = new MIPWinMMInput();
	if (!m_pInput->open(sampRate, channels, inputInterval))
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
	m_pOutput = new MIPWinMMOutput();
	if (!m_pOutput->open(sampRate, channels, outputInterval))
	{
		setErrorString(m_pOutput->getErrorString());
		deleteAll();
		return false;
	}
#else
	if (pParams2->getInputDevice() == pParams2->getOutputDevice())
	{
		MIPOSSInputOutputParams ioParams;
		
		m_pInput = new MIPOSSInputOutput();
		m_pOutput = m_pInput;

		ioParams.setDeviceName(pParams2->getInputDevice());

		if (!m_pInput->open(sampRate, channels, inputInterval, MIPOSSInputOutput::ReadWrite, &ioParams))
		{
			setErrorString(m_pInput->getErrorString());
			deleteAll();
			return false;
		}
	}
	else
	{
		MIPOSSInputOutputParams ioParams;
		
		m_pInput = new MIPOSSInputOutput();
		
		ioParams.setDeviceName(pParams2->getInputDevice());
		if (!m_pInput->open(sampRate, channels, inputInterval, MIPOSSInputOutput::ReadOnly, &ioParams))
		{
			setErrorString(m_pInput->getErrorString());
			deleteAll();
			return false;
		}

		m_pOutput = new MIPOSSInputOutput();
		
		ioParams.setDeviceName(pParams2->getOutputDevice());
		if (!m_pOutput->open(sampRate, channels, outputInterval, MIPOSSInputOutput::WriteOnly, &ioParams))
		{
			setErrorString(m_pOutput->getErrorString());
			deleteAll();
			return false;
		}
	}
#endif // WIN32 || _WIN32_WCE

#ifdef _WIN32_WCE
	m_pSplitter = new MIPAudioSplitter();
	if (!m_pSplitter->init(MIPTime(0.020)))
	{
		setErrorString(m_pSplitter->getErrorString());
		deleteAll();
		return false;
	}
#endif // _WIN32_WCE

	m_pSampEnc = new MIPSampleEncoder();
	if (!m_pSampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
	{
		setErrorString(m_pSampEnc->getErrorString());
		deleteAll();
		return false;
	}

	m_pSpeexEnc = new MIPSpeexEncoder();
	if (!m_pSpeexEnc->init(MIPSpeexEncoder::WideBand))
	{
		setErrorString(m_pSpeexEnc->getErrorString());
		deleteAll();
		return false;
	}

	m_pRTPEnc = new MIPRTPAudioEncoder();
	if (!m_pRTPEnc->init())
	{
		setErrorString(m_pRTPEnc->getErrorString());
		deleteAll();
		return false;
	}

	RTPUDPv4TransmissionParams transParams;
	RTPSessionParams sessParams;
	int status;
	
	transParams.SetPortbase(pParams2->getPortbase());
	sessParams.SetOwnTimestampUnit(1.0/((double)sampRate));
	sessParams.SetMaximumPacketSize(64000);
	sessParams.SetAcceptOwnPackets(pParams2->getAcceptOwnPackets());

	m_pRTPSession = new RTPSession();
	if ((status = m_pRTPSession->Create(sessParams,&transParams)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		deleteAll();
		return false;
	}

	m_pRTPComp = new MIPRTPComponent();
	if (!m_pRTPComp->init(m_pRTPSession))
	{
		setErrorString(m_pRTPComp->getErrorString());
		deleteAll();
		return false;
	}

	m_pTimer = new MIPAverageTimer(outputInterval);
	
	m_pRTPDec = new MIPRTPAudioDecoder();
	if (!m_pRTPDec->init(true, pSync))
	{
		setErrorString(m_pRTPDec->getErrorString());
		deleteAll();
		return false;
	}

	m_pMediaBuf = new MIPMediaBuffer();
	if (!m_pMediaBuf->init(outputInterval))
	{
		setErrorString(m_pMediaBuf->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pSpeexDec = new MIPSpeexDecoder();
	if (!m_pSpeexDec->init())
	{
		setErrorString(m_pSpeexDec->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pMixer = new MIPAudioMixer();
	if (!m_pMixer->init(sampRate, channels, outputInterval))
	{
		setErrorString(m_pMixer->getErrorString());
		deleteAll();
		return false;
	}

	uint32_t destType;
#if (defined(WIN32) || defined(_WIN32_WCE))
	destType = MIPRAWAUDIOMESSAGE_TYPE_S16LE;
#else
	destType = m_pOutput->getRawAudioSubtype();
#endif // WIN32 || _WIN32_WCE
	
	m_pSampEnc2 = new MIPSampleEncoder();
	if (!m_pSampEnc2->init(destType))
	{
		setErrorString(m_pSampEnc2->getErrorString());
		deleteAll();
		return false;
	}

	// Create input chain
	
	m_pInputChain = new InputChain(this);

	m_pInputChain->setChainStart(m_pInput);
#ifdef _WIN32_WCE
	m_pInputChain->addConnection(m_pInput, m_pSplitter);
	m_pInputChain->addConnection(m_pSplitter, m_pSampEnc);
#else
	m_pInputChain->addConnection(m_pInput, m_pSampEnc);
#endif // _WIN32_WCE
	m_pInputChain->addConnection(m_pSampEnc, m_pSpeexEnc);
	m_pInputChain->addConnection(m_pSpeexEnc, m_pRTPEnc);
	m_pInputChain->addConnection(m_pRTPEnc, m_pRTPComp);
	
	m_pOutputChain = new OutputChain(this);

	m_pOutputChain->setChainStart(m_pTimer);
	m_pOutputChain->addConnection(m_pTimer, m_pRTPComp);
	m_pOutputChain->addConnection(m_pRTPComp, m_pRTPDec);
	m_pOutputChain->addConnection(m_pRTPDec, m_pMediaBuf, true);
	m_pOutputChain->addConnection(m_pMediaBuf, m_pSpeexDec, true, MIPMESSAGE_TYPE_AUDIO_ENCODED, MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX);
	m_pOutputChain->addConnection(m_pSpeexDec, m_pMixer, true);
	m_pOutputChain->addConnection(m_pMixer, m_pSampEnc2, true);
	m_pOutputChain->addConnection(m_pSampEnc2, m_pOutput, true);

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
	return true;
}

bool MIPAudioSession::addDestination(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;
	if ((status = m_pRTPSession->AddDestination(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
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
	if ((status = m_pRTPSession->DeleteDestination(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	return true;
}

bool MIPAudioSession::clearDestinations()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->ClearDestinations();
	return true;
}

bool MIPAudioSession::supportsMulticasting()
{
	if (!m_init)
		return false;
	
	return m_pRTPSession->SupportsMulticasting();
}

bool MIPAudioSession::joinMulticastGroup(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	int status;
	if ((status = m_pRTPSession->JoinMulticastGroup(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
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
	if ((status = m_pRTPSession->LeaveMulticastGroup(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	return true;
}

bool MIPAudioSession::leaveAllMulticastGroups()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->LeaveAllMulticastGroups();
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
	if ((status = m_pRTPSession->SetReceiveMode(m)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
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
	if ((status = m_pRTPSession->AddToIgnoreList(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
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
	if ((status = m_pRTPSession->DeleteFromIgnoreList(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	return true;
}

bool MIPAudioSession::clearIgnoreList()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->ClearIgnoreList();
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
	if ((status = m_pRTPSession->AddToAcceptList(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
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
	if ((status = m_pRTPSession->DeleteFromAcceptList(addr)) < 0)
	{
		setErrorString(RTPGetErrorString(status));
		return false;
	}
	return true;
}

bool MIPAudioSession::clearAcceptList()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->ClearAcceptList();
	return true;
}

void MIPAudioSession::zeroAll()
{
	m_pInputChain = 0;
	m_pOutputChain = 0;
	m_pInput = 0;
	m_pOutput = 0;
	m_pSampEnc = 0;
	m_pSpeexEnc = 0;
	m_pRTPEnc = 0;
	m_pRTPComp = 0;
	m_pRTPSession = 0;
	m_pRTPDec = 0;
	m_pMediaBuf = 0;
	m_pSpeexDec = 0;
	m_pMixer = 0;
	m_pSampEnc2 = 0;
	m_pTimer = 0;
	m_pSplitter = 0;
}

void MIPAudioSession::deleteAll()
{
	if (m_pInputChain)
	{
		m_pInputChain->stop();
		delete m_pInputChain;
	}
	if (m_pOutputChain)
	{
		m_pOutputChain->stop();
		delete m_pOutputChain;
	}
#if (defined(WIN32) || defined(_WIN32_WCE))
	if (m_pInput)
		delete m_pInput;
	if (m_pOutput)
		delete m_pOutput;
#else
	if (m_pInput && m_pOutput)
	{
		if (m_pInput == m_pOutput)
			delete m_pInput;
		else
		{
			delete m_pInput;
			delete m_pOutput;
		}
	}
	else
	{
		if (m_pInput)
			delete m_pInput;
		if (m_pOutput)
			delete m_pOutput;
	}
#endif // WIN32 || _WIN32_WCE
	if (m_pSampEnc)
		delete m_pSampEnc;
	if (m_pSpeexEnc)
		delete m_pSpeexEnc;
	if (m_pRTPEnc)
		delete m_pRTPEnc;
	if (m_pRTPComp)
		delete m_pRTPComp;
	if (m_pRTPSession)
	{
		m_pRTPSession->Destroy();
		delete m_pRTPSession;
	}
	if (m_pRTPDec)
		delete m_pRTPDec;
	if (m_pMediaBuf)
		delete m_pMediaBuf;
	if (m_pSpeexDec)
		delete m_pSpeexDec;
	if (m_pMixer)
		delete m_pMixer;
	if (m_pSampEnc2)
		delete m_pSampEnc2;
	if (m_pTimer)
		delete m_pTimer;
	if (m_pSplitter)
		delete m_pSplitter;
	zeroAll();
}

#endif // MIPCONFIG_SUPPORT_SPEEX

