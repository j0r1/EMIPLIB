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

#if defined(MIPCONFIG_SUPPORT_AVCODEC) && (defined(MIPCONFIG_SUPPORT_DIRECTSHOW) || defined(MIPCONFIG_SUPPORT_VIDEO4LINUX))

#include "mipvideosession.h"
#if (defined(WIN32) || defined(_WIN32_WCE))
	#include "mipdirectshowcapture.h"
#else
	#include "mipv4linput.h"
#endif // WIN32 || _WIN32_WCE
#include "mipavcodecencoder.h"
#include "miprtpvideoencoder.h"
#include "miprtpcomponent.h"
#include "mipaveragetimer.h"
#include "miprtpdecoder.h"
#include "miprtpvideodecoder.h"
#include "mipmediabuffer.h"
#include "mipavcodecdecoder.h"
#include "mipvideomixer.h"
#include "mipqtoutput.h"
#include "mipencodedvideomessage.h"
#include "mipvideoframestorage.h"
#include <rtpsession.h>
#include <rtpsessionparams.h>
#include <rtperrors.h>
#include <rtpudpv4transmitter.h>

#define MIPVIDEOSESSION_ERRSTR_NOTINIT						"Not initialized"
#define MIPVIDEOSESSION_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPVIDEOSESSION_ERRSTR_NOQTSUPPORT					"No Qt support available"
#define MIPVIDEOSESSION_ERRSTR_NOSTORAGE					"The Qt component is being used instead of the storage component"

MIPVideoSession::MIPVideoSession()
{
	zeroAll();
	m_init = false;
}

MIPVideoSession::~MIPVideoSession()
{
	deleteAll();
}

bool MIPVideoSession::init(const MIPVideoSessionParams *pParams, MIPRTPSynchronizer *pSync)
{
	if (m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_ALREADYINIT);
		return false;
	}

	MIPVideoSessionParams defaultParams;
	const MIPVideoSessionParams *pParams2 = &defaultParams;

	if (pParams)
		pParams2 = pParams;

	real_t frameRate = pParams2->getFrameRate();
	int width;
	int height;
	
	m_pTimer = new MIPAverageTimer(MIPTime(1.0/frameRate));

#if (defined(WIN32) || defined(_WIN32_WCE))
	width = pParams2->getWidth();
	height = pParams2->getHeight();
	m_pInput = new MIPDirectShowCapture();
	if (!m_pInput->open(width, height, frameRate,pParams2->getDevice()))
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
#else
	std::string devName = pParams2->getDevice();
	m_pInput = new MIPV4LInput();
	if (!m_pInput->open(devName, 0))
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
	width = m_pInput->getWidth();
	height = m_pInput->getHeight();
#endif // WIN32 || _WIN32_WCE

	int bandwidth = pParams2->getBandwidth();
		
	m_pAvcEnc = new MIPAVCodecEncoder();
	if (!m_pAvcEnc->init(width, height, frameRate, bandwidth))
	{
		setErrorString(m_pAvcEnc->getErrorString());
		deleteAll();
		return false;
	}

	m_pRTPEnc = new MIPRTPVideoEncoder();
	if (!m_pRTPEnc->init(frameRate))
	{
		setErrorString(m_pRTPEnc->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pRTPEnc->setPayloadType(103);

	RTPUDPv4TransmissionParams transParams;
	RTPSessionParams sessParams;
	int status;
	
	transParams.SetPortbase(pParams2->getPortbase());
	sessParams.SetOwnTimestampUnit(1.0/90000.0);
	sessParams.SetMaximumPacketSize(64000);
	sessParams.SetAcceptOwnPackets(pParams2->getAcceptOwnPackets());

	m_pRTPSession = newRTPSession();
	if (m_pRTPSession == 0)
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

	m_pTimer2 = new MIPAverageTimer(MIPTime(1.0/frameRate));
	
	m_pRTPDec = new MIPRTPDecoder();
	if (!m_pRTPDec->init(true, pSync, m_pRTPSession))
	{
		setErrorString(m_pRTPDec->getErrorString());
		deleteAll();
		return false;
	}

	m_pRTPVideoDec = new MIPRTPVideoDecoder();
	m_pRTPDec->setDefaultPacketDecoder(m_pRTPVideoDec);

	m_pMediaBuf = new MIPMediaBuffer();
	if (!m_pMediaBuf->init(MIPTime(1.0/frameRate)))
	{
		setErrorString(m_pMediaBuf->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pAvcDec = new MIPAVCodecDecoder();
	if (!m_pAvcDec->init())
	{
		setErrorString(m_pAvcDec->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pMixer = new MIPVideoMixer();
	if (!m_pMixer->init(frameRate))
	{
		setErrorString(m_pMixer->getErrorString());
		deleteAll();
		return false;
	}

	if (pParams2->getUseQtOutput())
	{
#ifdef MIPCONFIG_SUPPORT_QT
		m_pQtOutput = new MIPQtOutput();
		if (!m_pQtOutput->init())
		{
			setErrorString(m_pQtOutput->getErrorString());
			deleteAll();
			return false;
		}
#else
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOQTSUPPORT);
		deleteAll();
		return false;
#endif // MIPCONFIG_SUPPORT_QT
	}
	else
	{
		m_pStorage = new MIPVideoFrameStorage();
		if (!m_pStorage->init())
		{
			setErrorString(m_pStorage->getErrorString());
			deleteAll();
			return false;
		}
	}
	
	// Create input chain
	
	m_pInputChain = new InputChain(this);

	m_pInputChain->setChainStart(m_pTimer);
	m_pInputChain->addConnection(m_pTimer, m_pInput);
	m_pInputChain->addConnection(m_pInput, m_pAvcEnc);
	m_pInputChain->addConnection(m_pAvcEnc, m_pRTPEnc);
	m_pInputChain->addConnection(m_pRTPEnc, m_pRTPComp);
	
	m_pOutputChain = new OutputChain(this);

	m_pOutputChain->setChainStart(m_pTimer2);
	m_pOutputChain->addConnection(m_pTimer2, m_pRTPComp);
	m_pOutputChain->addConnection(m_pRTPComp, m_pRTPDec);
	m_pOutputChain->addConnection(m_pRTPDec, m_pMediaBuf, true);
	m_pOutputChain->addConnection(m_pMediaBuf, m_pAvcDec, true, MIPMESSAGE_TYPE_VIDEO_ENCODED, MIPENCODEDVIDEOMESSAGE_TYPE_H263P);
	m_pOutputChain->addConnection(m_pAvcDec, m_pMixer, true);

#ifdef MIPCONFIG_SUPPORT_QT
	if (pParams2->getUseQtOutput())
		m_pOutputChain->addConnection(m_pMixer, m_pQtOutput, true);
	else
		m_pOutputChain->addConnection(m_pMixer, m_pStorage, true);
#else
	m_pOutputChain->addConnection(m_pMixer, m_pStorage, true);
#endif // MIPCONFIG_SUPPORT_QT
	
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

bool MIPVideoSession::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	deleteAll();
	m_init = false;
	return true;
}

bool MIPVideoSession::addDestination(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::deleteDestination(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::clearDestinations()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->ClearDestinations();
	return true;
}

bool MIPVideoSession::supportsMulticasting()
{
	if (!m_init)
		return false;
	
	return m_pRTPSession->SupportsMulticasting();
}

bool MIPVideoSession::joinMulticastGroup(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::leaveMulticastGroup(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::leaveAllMulticastGroups()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->LeaveAllMulticastGroups();
	return true;
}

bool MIPVideoSession::setReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::addToIgnoreList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::deleteFromIgnoreList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::clearIgnoreList()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->ClearIgnoreList();
	return true;
}

bool MIPVideoSession::addToAcceptList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::deleteFromAcceptList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::clearAcceptList()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPSession->ClearAcceptList();
	return true;
}

void MIPVideoSession::zeroAll()
{
	m_pInputChain = 0;
	m_pOutputChain = 0;
	m_pInput = 0;
	m_pAvcEnc = 0;
	m_pRTPEnc = 0;
	m_pRTPComp = 0;
	m_pRTPSession = 0;
	m_pTimer = 0;
	m_pTimer2 = 0;
	m_pRTPDec = 0;
	m_pRTPVideoDec = 0;
	m_pMediaBuf = 0;
	m_pAvcDec = 0;
	m_pMixer = 0;
	m_pQtOutput = 0;
	m_pStorage = 0;
}

void MIPVideoSession::deleteAll()
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
	if (m_pInput)
		delete m_pInput;
#ifdef MIPCONFIG_SUPPORT_QT
	if (m_pQtOutput)
		delete m_pQtOutput;
#endif // MIPCONFIG_SUPPORT_QT
	if (m_pStorage)
		delete m_pStorage;
	if (m_pTimer)
		delete m_pTimer;
	if (m_pAvcEnc)
		delete m_pAvcEnc;
	if (m_pRTPEnc)
		delete m_pRTPEnc;
	if (m_pRTPComp)
		delete m_pRTPComp;
	if (m_pRTPSession)
	{
		m_pRTPSession->BYEDestroy(RTPTime(2,0),0,0);
		delete m_pRTPSession;
	}
	if (m_pTimer2)
		delete m_pTimer2;
	if (m_pRTPDec)
		delete m_pRTPDec;
	if (m_pRTPVideoDec)
		delete m_pRTPVideoDec;
	if (m_pMediaBuf)
		delete m_pMediaBuf;
	if (m_pAvcDec)
		delete m_pAvcDec;
	if (m_pMixer)
		delete m_pMixer;
	zeroAll();
}

bool MIPVideoSession::getSourceIDs(std::list<uint64_t> &sourceIDs)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	if (m_pStorage == 0)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOSTORAGE);
		return false;
	}
	m_pStorage->lock();
	bool ret = m_pStorage->getSourceIDs(sourceIDs);
	if (!ret)
	{
		setErrorString(m_pStorage->getErrorString());
		m_pStorage->unlock();
		return false;
	}
	m_pStorage->unlock();
	return true;
}

bool MIPVideoSession::getVideoFrame(uint64_t sourceID, uint8_t **pData, int *pWidth, int *pHeight, MIPTime minimalTime)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	if (m_pStorage == 0)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOSTORAGE);
		return false;
	}

	MIPTime frameTime;
	int w, h;
	bool ret;
	
	m_pStorage->lock();

	ret = m_pStorage->getData(sourceID, 0, &w, &h, &frameTime);
	if (!ret)
	{
		setErrorString(m_pStorage->getErrorString());
		m_pStorage->unlock();
		return false;
	}
	if (frameTime > minimalTime)
	{
		size_t length = (size_t)((w*h*3)/2);
		uint8_t *pDataBuffer = new uint8_t [length];
		ret = m_pStorage->getData(sourceID, pDataBuffer, 0, 0, 0);
		if (!ret)
		{
			setErrorString(m_pStorage->getErrorString());
			m_pStorage->unlock();
			return false;
		}
		*pData = pDataBuffer;
	}
	else
	{
		*pData = 0;
	}
	
	m_pStorage->unlock();
	
	*pWidth = w;
	*pHeight = h;

	return true;
}

#endif // MIPCONFIG_SUPPORT_AVCODEC && (MIPCONFIG_SUPPORT_DIRECTSHOW || MIPCONFIG_SUPPORT_VIDEO4LINUX)

