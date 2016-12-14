/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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
#include "miprtpcomponent.h"
#include "miprtpmessage.h"
#include "mipsystemmessage.h"
#include <rtpsession.h>
#include <rtpsourcedata.h>

#include "mipdebug.h"

#define MIPRTPCOMPONENT_ERRSTR_NOTINIT			"Component was not initialized"
#define MIPRTPCOMPONENT_ERRSTR_ALREADYINIT		"Component is already initialized"
#define MIPRTPCOMPONENT_ERRSTR_BADSESSIONPARAM		"The RTP session parameter cannot be NULL"
#define MIPRTPCOMPONENT_ERRSTR_BADMESSAGE		"Not a valid message"
#define MIPRTPCOMPONENT_ERRSTR_NORTPSESSION		"The RTP session is not created yet"
#define MIPRTPCOMPONENT_ERRSTR_RTPERROR			"Detected JRTPLIB error: "

MIPRTPComponent::MIPRTPComponent() : MIPComponent("MIPRTPComponent")
{
	m_pRTPSession = 0;
	m_msgIt = m_messages.begin();
}

MIPRTPComponent::~MIPRTPComponent()
{
	destroy();
}

bool MIPRTPComponent::init(RTPSession *pSess)
{
	if (pSess == 0)
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_BADSESSIONPARAM);
		return false;
	}

	if (m_pRTPSession != 0)
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_ALREADYINIT);
		return false;
	}
	
	m_pRTPSession = pSess;
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPRTPComponent::destroy()
{
	if (m_pRTPSession == 0)
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_NOTINIT);
		return false;
	}
	m_pRTPSession = 0;
	clearMessages();
	return true;
}

bool MIPRTPComponent::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pRTPSession == 0)
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_NOTINIT);
		return false;
	}
	if (!m_pRTPSession->IsActive())
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_NORTPSESSION);
		return false;
	}
	
	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_RTP && pMsg->getMessageSubtype() == MIPRTPMESSAGE_TYPE_SEND)
	{
		// Send message
		
		MIPRTPSendMessage *pRTPMsg = (MIPRTPSendMessage *)pMsg;
		int status;

		MIPTime sampInst = pRTPMsg->getSamplingInstant();
		MIPTime delay = MIPTime::getCurrentTime();
		delay -= sampInst;

		if (delay.getValue() > 0 && delay.getValue() < 10.0) // ok, we can assume the time was indeed the sampling instant
			m_pRTPSession->SetPreTransmissionDelay(RTPTime((uint32_t)delay.getSeconds(),(uint32_t)delay.getMicroSeconds()));

		status = m_pRTPSession->SendPacket(pRTPMsg->getPayload(),pRTPMsg->getPayloadLength(),
		                                pRTPMsg->getPayloadType(),pRTPMsg->getMarker(),
						pRTPMsg->getTimestampIncrement());
		if (status < 0)
		{
			setErrorString(std::string(MIPRTPCOMPONENT_ERRSTR_RTPERROR) + RTPGetErrorString(status));
			return false;
		}
	}
	else if (pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_ISTIME)
	{
		return true;
	}
	else
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPRTPComponent::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pRTPSession == 0)
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_NOTINIT);
		return false;
	}
	if (!m_pRTPSession->IsActive())
	{
		setErrorString(MIPRTPCOMPONENT_ERRSTR_NORTPSESSION);
		return false;
	}
	if (!processNewPackets(iteration))
		return false;

	if (m_msgIt == m_messages.end())
	{
		m_msgIt = m_messages.begin();
		*pMsg = 0;
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	return true;
}

bool MIPRTPComponent::processNewPackets(int64_t iteration)
{
	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
		m_pRTPSession->Poll(); // This is a dummy function if the RTPSession background thread is used.
		m_pRTPSession->BeginDataAccess();
		if (m_pRTPSession->GotoFirstSourceWithData())
		{
			do
			{
				RTPSourceData *srcData = m_pRTPSession->GetCurrentSourceInfo();
				
				size_t cnameLength;
				const uint8_t *pCName = srcData->SDES_GetCNAME(&cnameLength);
				uint32_t jitterSamples = srcData->INF_GetJitter();
				real_t jitterSeconds = 0;
				real_t tsUnit;
				real_t tsUnitEstimation;
				bool setTimingInfo = false;
				uint32_t timingInfTimestamp = 0;
				MIPTime timingInfWallclock(0);
				
				if ((tsUnit = (real_t)srcData->GetTimestampUnit()) > 0)
					jitterSeconds = (real_t)jitterSamples*tsUnit;
				else
				{
					if ((tsUnitEstimation = (real_t)srcData->INF_GetEstimatedTimestampUnit()) > 0)
						jitterSeconds = (real_t)jitterSamples*tsUnitEstimation;
				}
	
				if (srcData->SR_HasInfo())
				{
					RTPNTPTime ntpTime = srcData->SR_GetNTPTimestamp();
	
					if (!(ntpTime.GetMSW() == 0 && ntpTime.GetLSW() == 0))
					{
						setTimingInfo = true;
						RTPTime rtpTime(ntpTime);
						timingInfWallclock = MIPTime(rtpTime.GetSeconds(),rtpTime.GetMicroSeconds());
						timingInfTimestamp = srcData->SR_GetRTPTimestamp();
					}
				}
				
				RTPPacket *pPack;
				
				while ((pPack = m_pRTPSession->GetNextPacket()) != 0)
				{
					MIPRTPReceiveMessage *pRTPMsg = new MIPRTPReceiveMessage(pPack,pCName,cnameLength,true,m_pRTPSession);

					pRTPMsg->setJitter(MIPTime(jitterSeconds));
					if (tsUnit > 0)
						pRTPMsg->setTimestampUnit(tsUnit);
					if (setTimingInfo)
						pRTPMsg->setTimingInfo(timingInfWallclock, timingInfTimestamp);

					pRTPMsg->setSourceID(getSourceID(pPack, srcData));
					m_messages.push_back(pRTPMsg);
				}
			} while (m_pRTPSession->GotoNextSourceWithData());
		}
		m_pRTPSession->EndDataAccess();
  		m_msgIt = m_messages.begin();
	}
	return true;
}

void MIPRTPComponent::clearMessages()
{
	std::list<MIPRTPReceiveMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

uint64_t MIPRTPComponent::getSourceID(const RTPPacket *pPack, const RTPSourceData *pSourceData) const
{
	return (uint64_t)(pPack->GetSSRC());
}

