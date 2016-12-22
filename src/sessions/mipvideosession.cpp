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

#if defined(MIPCONFIG_SUPPORT_AVCODEC) && (defined(MIPCONFIG_SUPPORT_DIRECTSHOW) || defined(MIPCONFIG_SUPPORT_VIDEO4LINUX))

#include "mipvideosession.h"
#include "mipdirectshowcapture.h"
#include "mipv4l2input.h"
#include "miptinyjpegdecoder.h"
#include "mipavcodecframeconverter.h"
#include "mipavcodecencoder.h"
#include "miprtph263encoder.h"
#include "miprtpvideoencoder.h"
#include "miprtpcomponent.h"
#include "mipaveragetimer.h"
#include "miprtpdecoder.h"
#include "miprtph263decoder.h"
#include "miprtpvideodecoder.h"
#include "miprtpdummydecoder.h"
#include "mipmediabuffer.h"
#include "mipcomponentalias.h"
#include "mipavcodecdecoder.h"
#include "mipvideomixer.h"
#include "mipqt5outputwidget.h"
#include "mipencodedvideomessage.h"
#include "mipvideoframestorage.h"
#include "miprawvideomessage.h"
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpudpv4transmitter.h>

#include "mipdebug.h"

using namespace jrtplib;

#define MIPVIDEOSESSION_ERRSTR_NOTINIT						"Not initialized"
#define MIPVIDEOSESSION_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPVIDEOSESSION_ERRSTR_NOQTSUPPORT					"No Qt support available"
#define MIPVIDEOSESSION_ERRSTR_NOSTORAGE					"The Qt component is being used instead of the storage component"
#define MIPVIDEOSESSION_ERRSTR_NOOUTPUTCOMPONENT			"The Qt component is not being used"
#define MIPVIDEOSESSION_ERRSTR_CONFLICTPAYLOADTYPEMAPPING	"The incoming payload types for H263 and internal video formats cannot be the same"

MIPVideoSession::MIPVideoSession()
{
	zeroAll();
	m_init = false;
}

MIPVideoSession::~MIPVideoSession()
{
	deleteAll();
}

bool MIPVideoSession::init(const MIPVideoSessionParams *pParams, MIPRTPSynchronizer *pSync, RTPSession *pRTPSession,
                           bool autoStart)
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

	MIPVideoSessionParams::SessionType sessionType = pParams2->getSessionType();
	real_t frameRate = pParams2->getFrameRate();

	if (pParams2->getIncomingInternalPayloadType() == pParams2->getIncomingH263PayloadType())
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_CONFLICTPAYLOADTYPEMAPPING);
		return false;
	}

	if (sessionType == MIPVideoSessionParams::InputOutput)
	{
		int width;
		int height;
		
		m_pTimer = new MIPAverageTimer(MIPTime(1.0/frameRate));

#ifdef MIPCONFIG_SUPPORT_DIRECTSHOW
		width = pParams2->getWidth();
		height = pParams2->getHeight();
		m_pInput = new MIPDirectShowCapture();
		if (!m_pInput->open(width, height, frameRate,pParams2->getDeviceNumber()))
		{
			setErrorString(m_pInput->getErrorString());
			deleteAll();
			return false;
		}

		if (m_pInput->getFrameSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUYV)
		{
			m_pInputFrameConverter = new MIPAVCodecFrameConverter();

			if (!m_pInputFrameConverter->init(width, height, MIPRAWVIDEOMESSAGE_TYPE_YUV420P))
			{
				setErrorString(m_pInputFrameConverter->getErrorString());
				deleteAll();
				return false;
			}
		}
#else
		width = pParams2->getWidth();
		height = pParams2->getHeight();
		std::string devName = pParams2->getDeviceName();

		m_pInput = new MIPV4L2Input();
		if (!m_pInput->open(width, height, devName))
		{
			setErrorString(m_pInput->getErrorString());
			deleteAll();
			return false;
		}
		width = m_pInput->getWidth();
		height = m_pInput->getHeight();

		if (m_pInput->getCompressed())
		{
			m_pTinyJpegDec = new MIPTinyJPEGDecoder();

			if (!m_pTinyJpegDec->init())
			{
				setErrorString(m_pTinyJpegDec->getErrorString());
				deleteAll();
				return false;
			}
		}
		else
		{
			if (m_pInput->getFrameSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUYV)
			{
				m_pInputFrameConverter = new MIPAVCodecFrameConverter();

				if (!m_pInputFrameConverter->init(width, height, MIPRAWVIDEOMESSAGE_TYPE_YUV420P))
				{
					setErrorString(m_pInputFrameConverter->getErrorString());
					deleteAll();
					return false;
				}
			}
		}
#endif // MIPCONFIG_SUPPORT_DIRECTSHOW

		int bandwidth = pParams2->getBandwidth();
			
		if (pParams2->getEncodingType() != MIPVideoSessionParams::IntYUV420)
		{
			m_pAvcEnc = new MIPAVCodecEncoder();
			if (!m_pAvcEnc->init(width, height, frameRate, bandwidth))
			{
				setErrorString(m_pAvcEnc->getErrorString());
				deleteAll();
				return false;
			}
		}

		if (pParams2->getEncodingType() == MIPVideoSessionParams::H263)
		{
			m_pRTPH263Enc = new MIPRTPH263Encoder();

			if (!m_pRTPH263Enc->init(frameRate, pParams2->getMaximumPayloadSize()))
			{
				setErrorString(m_pRTPH263Enc->getErrorString());
				deleteAll();
				return false;
			}
		
			m_pRTPH263Enc->setPayloadType(pParams2->getOutgoingH263PayloadType());
		}
		else
		{
			MIPRTPVideoEncoder::EncodingType encType;

			if (pParams2->getEncodingType() == MIPVideoSessionParams::IntH263)
				encType = MIPRTPVideoEncoder::H263;
			else // assume YUV420P
				encType = MIPRTPVideoEncoder::YUV420P;

			m_pRTPIntVideoEnc = new MIPRTPVideoEncoder();
			if (!m_pRTPIntVideoEnc->init(frameRate, pParams2->getMaximumPayloadSize(), encType))
			{
				setErrorString(m_pRTPIntVideoEnc->getErrorString());
				deleteAll();
				return false;
			}
		
			m_pRTPIntVideoEnc->setPayloadType(pParams2->getOutgoingInternalPayloadType());
		}
	}

	if (pRTPSession != 0)
	{
		int status;
		
		m_pRTPSession = pRTPSession;
		m_deleteRTPSession = false;

		if ((status = m_pRTPSession->SetTimestampUnit(1.0/90000.0)) < 0)
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
		transParams.SetRTPSendBuffer(1024*1024);// TODO: this ok?
		transParams.SetRTPReceiveBuffer(1024*1024); // TODO: this ok?
		sessParams.SetOwnTimestampUnit(1.0/90000.0);
		sessParams.SetMaximumPacketSize(pParams2->getMaximumPayloadSize()+12); // account for RTP header
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

	m_pRTPDummyDec = new MIPRTPDummyDecoder();
	m_pRTPDec->setDefaultPacketDecoder(m_pRTPDummyDec);

	m_pRTPH263Dec = new MIPRTPH263Decoder();
	m_pRTPDec->setPacketDecoder(pParams2->getIncomingH263PayloadType(), m_pRTPH263Dec);

	m_pRTPIntVideoDec = new MIPRTPVideoDecoder();
	m_pRTPDec->setPacketDecoder(pParams2->getIncomingInternalPayloadType(), m_pRTPIntVideoDec);

	m_pMediaBuf = new MIPMediaBuffer();
	if (!m_pMediaBuf->init(MIPTime(1.0/frameRate)))
	{
		setErrorString(m_pMediaBuf->getErrorString());
		deleteAll();
		return false;
	}
	
	m_pBufferAlias = new MIPComponentAlias(m_pMediaBuf);
	
	m_pAvcDec = new MIPAVCodecDecoder();
	if (!m_pAvcDec->init(pParams2->getWaitForKeyframe()))
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
#ifdef MIPCONFIG_SUPPORT_QT5
		m_pOutputFrameConverter = new MIPAVCodecFrameConverter();
		if (!m_pOutputFrameConverter->init(-1, -1, MIPRAWVIDEOMESSAGE_TYPE_RGB32))
		{
			setErrorString(m_pOutputFrameConverter->getErrorString());
			deleteAll();
			return false;
		}

		m_pOutputComponent = new MIPQt5OutputComponent();
		if (!m_pOutputComponent->init())
		{
			setErrorString(m_pOutputComponent->getErrorString());
			deleteAll();
			return false;
		}
#else
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOQTSUPPORT);
		deleteAll();
		return false;
#endif // MIPCONFIG_SUPPORT_QT5
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
	
	if (sessionType == MIPVideoSessionParams::InputOutput)
	{
		// Create input chain
		
		m_pInputChain = new InputChain(this);

		m_pInputChain->setChainStart(m_pTimer);
		m_pInputChain->addConnection(m_pTimer, m_pInput);

		MIPComponent *pRTPEnc = 0;

		if (pParams2->getEncodingType() == MIPVideoSessionParams::H263)
			pRTPEnc = m_pRTPH263Enc;
		else
			pRTPEnc = m_pRTPIntVideoEnc;

		if (m_pTinyJpegDec == 0)
		{
			if (m_pInputFrameConverter == 0)
			{
				if (pParams2->getEncodingType() == MIPVideoSessionParams::IntYUV420)
					m_pInputChain->addConnection(m_pInput, pRTPEnc);
				else
					m_pInputChain->addConnection(m_pInput, m_pAvcEnc);
			}
			else
			{
				m_pInputChain->addConnection(m_pInput, m_pInputFrameConverter);

				if (pParams2->getEncodingType() == MIPVideoSessionParams::IntYUV420)
					m_pInputChain->addConnection(m_pInputFrameConverter, pRTPEnc);
				else
					m_pInputChain->addConnection(m_pInputFrameConverter, m_pAvcEnc);
			}
		}
		else
		{
			m_pInputChain->addConnection(m_pInput, m_pTinyJpegDec);

			if (pParams2->getEncodingType() == MIPVideoSessionParams::IntYUV420)
				m_pInputChain->addConnection(m_pTinyJpegDec, pRTPEnc);
			else
				m_pInputChain->addConnection(m_pTinyJpegDec, m_pAvcEnc);
		}

		if (pParams2->getEncodingType() != MIPVideoSessionParams::IntYUV420)
			m_pInputChain->addConnection(m_pAvcEnc, pRTPEnc);

		m_pInputChain->addConnection(pRTPEnc, m_pRTPComp);
	}
	
	m_pOutputChain = new OutputChain(this);

	m_pOutputChain->setChainStart(m_pTimer2);
	m_pOutputChain->addConnection(m_pTimer2, m_pRTPComp);

	m_pOutputChain->addConnection(m_pRTPComp, m_pRTPDec);
	m_pOutputChain->addConnection(m_pRTPDec, m_pMediaBuf, true);
	m_pOutputChain->addConnection(m_pMediaBuf, m_pAvcDec, true, MIPMESSAGE_TYPE_VIDEO_ENCODED, MIPENCODEDVIDEOMESSAGE_TYPE_H263P);
	m_pOutputChain->addConnection(m_pAvcDec, m_pMixer, true);
	
	m_pOutputChain->addConnection(m_pMediaBuf, m_pBufferAlias, false, 0, 0); // extra link to creat equal length branches
	m_pOutputChain->addConnection(m_pBufferAlias, m_pMixer, false, MIPMESSAGE_TYPE_VIDEO_RAW, MIPRAWVIDEOMESSAGE_TYPE_YUV420P);
 
#ifdef MIPCONFIG_SUPPORT_QT5
	if (pParams2->getUseQtOutput())
	{
		m_pOutputChain->addConnection(m_pMixer, m_pOutputFrameConverter, true);
		m_pOutputChain->addConnection(m_pOutputFrameConverter, m_pOutputComponent, true);
	}
	else
		m_pOutputChain->addConnection(m_pMixer, m_pStorage, true);
#else
	m_pOutputChain->addConnection(m_pMixer, m_pStorage, true);
#endif // MIPCONFIG_SUPPORT_QT5

	m_init = true; // needed for startChain to work

	if (autoStart)
	{
		if (!startChain()) // error string was already set
		{
			m_init = false;
			deleteAll();
			return false;
		}
	}
	
	return true;
}

bool MIPVideoSession::startChain()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}

	if (m_pInputChain)
	{
		if (!m_pInputChain->start())
		{
			setErrorString(m_pInputChain->getErrorString());
			return false;
		}
	}

	if (!m_pOutputChain->start())
	{
		setErrorString(m_pOutputChain->getErrorString());
		return false;
	}
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

// Note: since we can't be sure that the underlying RTPTransmitter
// was compiled in a thread-safe way, we'll lock the RTP component

bool MIPVideoSession::addDestination(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::deleteDestination(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::clearDestinations()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->ClearDestinations();
	m_pRTPComp->unlock();
	return true;
}

bool MIPVideoSession::supportsMulticasting()
{
	if (!m_init)
		return false;
	
	m_pRTPComp->lock();
	bool val = m_pRTPSession->SupportsMulticasting();
	m_pRTPComp->unlock();

	return val;
}

bool MIPVideoSession::joinMulticastGroup(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::leaveMulticastGroup(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::leaveAllMulticastGroups()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->LeaveAllMulticastGroups();
	m_pRTPComp->unlock();
	
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

bool MIPVideoSession::addToIgnoreList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::deleteFromIgnoreList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::clearIgnoreList()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->ClearIgnoreList();
	m_pRTPComp->unlock();
	
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

bool MIPVideoSession::deleteFromAcceptList(const RTPAddress &addr)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
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

bool MIPVideoSession::clearAcceptList()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return false;
	}
	
	m_pRTPComp->lock();
	m_pRTPSession->ClearAcceptList();
	m_pRTPComp->unlock();
	
	return true;
}

void MIPVideoSession::zeroAll()
{
	m_pInputChain = 0;
	m_pOutputChain = 0;
	m_pInput = 0;
	m_pTinyJpegDec = 0;
	m_pInputFrameConverter = 0;
	m_pOutputFrameConverter = 0;
	m_pAvcEnc = 0;
	m_pRTPH263Enc = 0;
	m_pRTPIntVideoEnc = 0;
	m_pRTPComp = 0;
	m_pRTPSession = 0;
	m_pTimer = 0;
	m_pTimer2 = 0;
	m_pRTPDec = 0;
	m_pRTPH263Dec = 0;
	m_pRTPIntVideoDec = 0;
	m_pRTPDummyDec = 0;
	m_pMediaBuf = 0;
	m_pBufferAlias = 0;
	m_pAvcDec = 0;
	m_pMixer = 0;
	m_pOutputComponent = 0;
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
	if (m_pTinyJpegDec)
		delete m_pTinyJpegDec;
	if (m_pInputFrameConverter)
		delete m_pInputFrameConverter;
	if (m_pOutputFrameConverter)
		delete m_pOutputFrameConverter;
#ifdef MIPCONFIG_SUPPORT_QT5
	if (m_pOutputComponent)
		delete m_pOutputComponent;
#endif // MIPCONFIG_SUPPORT_QT5
	if (m_pStorage)
		delete m_pStorage;
	if (m_pTimer)
		delete m_pTimer;
	if (m_pAvcEnc)
		delete m_pAvcEnc;
	if (m_pRTPH263Enc)
		delete m_pRTPH263Enc;
	if (m_pRTPIntVideoEnc)
		delete m_pRTPIntVideoEnc;
	if (m_pRTPComp)
		delete m_pRTPComp;
	if (m_pRTPSession)
	{
		if (m_deleteRTPSession)
		{
			m_pRTPSession->BYEDestroy(RTPTime(2,0),0,0);
			delete m_pRTPSession;
		}
	}
	if (m_pTimer2)
		delete m_pTimer2;
	if (m_pRTPDec)
		delete m_pRTPDec;
	if (m_pRTPH263Dec)
		delete m_pRTPH263Dec;
	if (m_pRTPIntVideoDec)
		delete m_pRTPIntVideoDec;
	if (m_pRTPDummyDec)
		delete m_pRTPDummyDec;
	if (m_pMediaBuf)
		delete m_pMediaBuf;
	if (m_pBufferAlias)
		delete m_pBufferAlias;
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

MIPQt5OutputComponent *MIPVideoSession::getQt5OutputComponent()
{
#ifdef MIPCONFIG_SUPPORT_QT5
	if (!m_init)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOTINIT);
		return nullptr;
	}
	if (!m_pOutputComponent)
	{
		setErrorString(MIPVIDEOSESSION_ERRSTR_NOOUTPUTCOMPONENT);
		return nullptr;
	}
	return m_pOutputComponent;
#else
	setErrorString(MIPVIDEOSESSION_ERRSTR_NOQTSUPPORT);
	return nullptr;
#endif
}

#endif // MIPCONFIG_SUPPORT_AVCODEC && (MIPCONFIG_SUPPORT_DIRECTSHOW || MIPCONFIG_SUPPORT_VIDEO4LINUX)

