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
#include "miprtpdecoder.h"
#include "miprawaudiomessage.h"
#include "miprtpmessage.h"
#include "mipfeedback.h"
#include "miprtpsynchronizer.h"
#include "mipmediamessage.h"
#include "miprtppacketdecoder.h"
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsourcedata.h>

#include "mipdebug.h"

using namespace jrtplib;

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

#define MIPRTPDECODER_ERRSTR_NOTINIT				"Not initialized"
#define MIPRTPDECODER_ERRSTR_BADMESSAGE				"Bad message"
#define MIPRTPDECODER_ERRSTR_NOPACKETDECODERINSTALLED		"No RTP packet decoder installed for received payload type"

MIPRTPDecoder::MIPRTPDecoder() : MIPComponent("MIPRTPDecoder"), m_playbackOffset(0), m_prevCleanTableTime(0), m_maxJitterBuffer(-1)
{
	m_init = false;
}

MIPRTPDecoder::~MIPRTPDecoder()
{
	cleanUp();
}

bool MIPRTPDecoder::init(bool calcStreamTime, MIPRTPSynchronizer *pSynchronizer, RTPSession *pRTPSess)
{
	if (m_init)
		cleanUp();

	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_gotPlaybackFeedback = false;
	m_prevCleanTableTime = MIPTime::getCurrentTime();
	m_calcStreamTime = calcStreamTime;
	m_pSynchronizer = pSynchronizer;
	m_totalComponentDelay = MIPTime(0);
	m_pRTPSess = pRTPSess;

	for (int i = 0 ; i < MIPRTPDECODER_MAXPAYLOADDECODERS ; i++)
		m_pDecoders[i] = 0;
		
	m_init = true;
	return true;
}

bool MIPRTPDecoder::setDefaultPacketDecoder(MIPRTPPacketDecoder *pDec)
{
	if (!m_init)
	{
		setErrorString(MIPRTPDECODER_ERRSTR_NOTINIT);
		return false;
	}
	for (int i = 0 ; i < MIPRTPDECODER_MAXPAYLOADDECODERS ; i++)
		m_pDecoders[i] = pDec;
	return true;
}

bool MIPRTPDecoder::setPacketDecoder(uint8_t payloadType, MIPRTPPacketDecoder *pDec)
{
	if (!m_init)
	{
		setErrorString(MIPRTPDECODER_ERRSTR_NOTINIT);
		return false;
	}
	m_pDecoders[(int)payloadType] = pDec;
	return true;
}

bool MIPRTPDecoder::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPDECODER_ERRSTR_NOTINIT);
		return false;
	}
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_RTP && pMsg->getMessageSubtype() == MIPRTPMESSAGE_TYPE_RECEIVE))
	{
		setErrorString(MIPRTPDECODER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
		cleanUpSourceTable();
	}

	if (m_calcStreamTime)
	{
		if (!m_gotPlaybackFeedback) // we don't have any information about the playback stream, ignore packet
			return true;
	}

	MIPRTPReceiveMessage *pRTPMsg = (MIPRTPReceiveMessage *)pMsg;
	const RTPPacket *pRTPPack = pRTPMsg->getPacket();
	real_t timestampUnit = pRTPMsg->getTimestampUnit();
	MIPRTPPacketDecoder *pDecoder = m_pDecoders[(int)(pRTPPack->GetPayloadType())];

	if (pDecoder == 0)
	{
		setErrorString(MIPRTPDECODER_ERRSTR_NOPACKETDECODERINSTALLED);
		return false;
	}
	
	//std::cerr << "Got packet " << pRTPPack->GetExtendedSequenceNumber() << " from source " << pRTPPack->GetSSRC() << std::endl;

	real_t timestampUnitPrev = timestampUnit;
	real_t timestampUnitEstimate = pRTPMsg->getTimestampUnitEstimate();
	
	if (!pDecoder->validatePacket(pRTPPack, timestampUnit, timestampUnitEstimate))
	{
		// packet type not understood, ignore packet
		return true;
	}

	if (m_pRTPSess)
	{
		if (timestampUnitPrev != timestampUnit && timestampUnit > 0)
		{
			m_pRTPSess->BeginDataAccess();
			RTPSourceData *pSrcDat = m_pRTPSess->GetSourceInfo(pRTPPack->GetSSRC());
			if (pSrcDat != 0)
			{
				pSrcDat->SetTimestampUnit((double)timestampUnit);
//				std::cout << "Storing timestamp unit 1.0/" << (1.0/timestampUnit) << " for SSRC" << pRTPPack->GetSSRC() << std::endl;
			}
			m_pRTPSess->EndDataAccess();
		}
	}
	
	MIPTime streamTime(0);
	
	if (m_calcStreamTime)
	{
		if (timestampUnit <= 0)
		{
			// without the timestamp unit we can't reconstruct the stream, so we'll
			// simply ignore this packet
			return true;
		}
	}

	std::list<MIPMediaMessage *> messages;
	std::list<MIPMediaMessage *>::iterator it;
	std::list<uint32_t> timestamps;
	std::list<uint32_t>::iterator it2;

	pDecoder->createNewMessages(pRTPPack, messages, timestamps);

	if (messages.empty())
	{
		// Either something went wrong, or the RTP packet was only part of a composite packet
		// Ignore packet
		return true;
	}

	uint64_t sourceID = pRTPMsg->getSourceID();
	uint32_t ssrc = pRTPPack->GetSSRC();
	const uint8_t *pCName = pRTPMsg->getCName();
	size_t cnameLength = pRTPMsg->getCNameLength();
	MIPTime jitterValue = pRTPMsg->getJitter();

	for (it = messages.begin(), it2 = timestamps.begin() ; it != messages.end() ; it++, it2++)
	{
		MIPMediaMessage *pNewMsg = *it;

		if (m_calcStreamTime)
		{
			bool shouldSync = false;
			uint32_t timestamp = *it2;

			if (!lookUpStreamTime(ssrc, timestamp, pCName, cnameLength, timestampUnit, streamTime, shouldSync))
			{
				// something went wrong, ignore packet
				return true;
			}

			MIPTime insertOffset(0);

			if (!adjustToPlaybackTime(jitterValue, streamTime, insertOffset))
			{
				// something went wrong, ignore packet
				return true;
			}

			if (m_pSynchronizer != 0) // a synchronization object is available
			{
				MIPTime syncOffset;

				if (shouldSync)
				{
					m_pSynchronizer->lock();
					if (pRTPMsg->isTimingInfoSet())
					{
						uint32_t SRtimestamp;
						MIPTime SRwallclock(0);
					
						pRTPMsg->getTimingInfo(&SRwallclock,&SRtimestamp);
						m_pSynchronizer->setStreamInfo(m_pSSRCInfo->getSyncStreamID(), SRwallclock, SRtimestamp,
								       timestamp, insertOffset, m_totalComponentDelay);
					}
					syncOffset = m_pSynchronizer->calculateSynchronizationOffset(m_pSSRCInfo->getSyncStreamID());
					m_pSynchronizer->unlock();
				
					m_pSSRCInfo->setSyncOffset(syncOffset);
				}
				else
					syncOffset = m_pSSRCInfo->getSyncOffset();

				streamTime += syncOffset;
			}
		}
		
		pNewMsg->setSourceID(sourceID);
		pNewMsg->setTime(streamTime);
		
		m_messages.push_back(pNewMsg);
	}

	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPRTPDecoder::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPRTPDECODER_ERRSTR_NOTINIT);
		return false;
	}	
	
	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
		cleanUpSourceTable();
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

bool MIPRTPDecoder::processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback)
{
	if (!m_init)
	{
		setErrorString(MIPRTPDECODER_ERRSTR_NOTINIT);
		return false;
	}	
	
	if (feedback->hasPlaybackStreamTime())
	{
		m_gotPlaybackFeedback = true;
		m_playbackOffset = feedback->getPlaybackStreamTime();
	}

	m_totalComponentDelay = feedback->getPlaybackDelay();
	
	return true;
}
   
void MIPRTPDecoder::cleanUp()
{
	clearMessages();
	m_sourceTable.clear();
	m_init = false;
}

void MIPRTPDecoder::clearMessages()
{
	std::list<MIPMediaMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPRTPDecoder::cleanUpSourceTable()
{
	MIPTime curTime = MIPTime::getCurrentTime();

	if ((curTime.getValue() - m_prevCleanTableTime.getValue()) < 60.0) // only cleanup every 60 seconds
		return;
	
	hash_map<uint32_t, SSRCInfo>::iterator it;
	
	it = m_sourceTable.begin();
	while (it != m_sourceTable.end())
	{
		if ((curTime.getValue() - (*it).second.getLastAccessTime().getValue() ) < 60.0)
			it++;
		else
		{
			hash_map<uint32_t, SSRCInfo>::iterator it2;

			it2 = it;
			it++;
			if (m_pSynchronizer && (*it2).second.getSyncStreamID() >= 0)
			{
				m_pSynchronizer->lock();
				m_pSynchronizer->unregisterStream((*it2).second.getSyncStreamID());
				m_pSynchronizer->unlock();
			}
			m_sourceTable.erase(it2);
		}
	}
	m_prevCleanTableTime = curTime;
}

bool MIPRTPDecoder::lookUpStreamTime(uint32_t ssrc, uint32_t timestamp, const uint8_t *pCName, size_t cnameLength, real_t timestampUnit, MIPTime &streamTime, bool &shouldSync)
{
	MIPTime curTime = MIPTime::getCurrentTime();
	hash_map<uint32_t, SSRCInfo>::iterator it = m_sourceTable.find(ssrc);
	
	if (it == m_sourceTable.end())
	{
		m_sourceTable[ssrc] = SSRCInfo(timestampUnit);
		m_sourceTable[ssrc].setLastAccessTime(curTime);
		m_pSSRCInfo = &(m_sourceTable[ssrc]);
		streamTime = MIPTime(0);

		if (m_pSynchronizer != 0 && cnameLength > 0)
		{
			int64_t id;
			
			m_pSynchronizer->lock();
			if (m_pSynchronizer->registerStream(pCName, cnameLength, timestampUnit, &id))
				m_pSSRCInfo->setSyncStreamID(id);
			m_pSynchronizer->unlock();
		}
	}
	else
	{
		uint64_t extendedTimestamp;
		uint64_t baseTimestamp,diff;
		
		m_pSSRCInfo = &((*it).second);

		if (m_pSynchronizer != 0 && m_pSSRCInfo->getSyncStreamID() < 0 && cnameLength > 0)
		{
			int64_t id;
			
			m_pSynchronizer->lock();
			if (m_pSynchronizer->registerStream(pCName, cnameLength, timestampUnit, &id))
				m_pSSRCInfo->setSyncStreamID(id);
			m_pSynchronizer->unlock();
		}
		
		(*it).second.setLastAccessTime(curTime);
		extendedTimestamp = (*it).second.getExtendedTimestamp(timestamp);
		baseTimestamp = (*it).second.getBaseTimestamp();
		diff = extendedTimestamp-baseTimestamp;
		
		streamTime = MIPTime(((real_t)diff)*timestampUnit);
	}

	MIPTime diff = curTime;
	diff -= m_pSSRCInfo->getLastSyncTime();
	
	if (diff.getValue() > 2.0)
	{
		shouldSync = true;
		m_pSSRCInfo->setLastSyncTime(curTime);
	}
	else
		shouldSync = false;

	return true;
}

#define MINOFFSET 0.000005

bool MIPRTPDecoder::adjustToPlaybackTime(MIPTime jitterValue, MIPTime &streamTime, MIPTime &insertOffset)
{
	// The current SSRCInfo is in m_pSSRCInfo;
	
	if (!m_pSSRCInfo->hasLastOffsetAdjustTime())
	{
		MIPTime defaultOffset = MIPTime(jitterValue.getValue()*2.0);

		defaultOffset += MIPTime(MINOFFSET);
		defaultOffset += m_playbackOffset;
		
		m_pSSRCInfo->setPlaybackOffset(defaultOffset);
	}
	
	streamTime += m_pSSRCInfo->getPlaybackOffset();

	// TODO: search for a decent jitter buffer algorithm
	
	// Jitter buffer

	MIPTime diff = streamTime;
	diff -= m_playbackOffset;
//	std::cerr << "Diff:            " << diff.getString() << std::endl;
	
	m_pSSRCInfo->addInsertTime(diff);

//	std::cerr << "Insert average:  " << m_pSSRCInfo->getInsertTimeAverage().getString() << std::endl;
//	std::cerr << "Insert variance: " << m_pSSRCInfo->getInsertTimeVariance().getString() << std::endl;
//	std::cerr << "Jitter:          " << jitterValue.getString() << std::endl;
	
	real_t insertDiff = m_pSSRCInfo->getInsertTimeAverage().getValue();
	real_t variance = m_pSSRCInfo->getInsertTimeVariance().getValue();
//	real_t jitter = jitterValue.getValue();
//
//	if (jitter > variance)
//		variance = jitter;

	//real_t offset = insertDiff - 2.0*variance;
	real_t offset = insertDiff - 3.0*variance; // TODO: which is best

	// Limit the maximum offset
	if (m_maxJitterBuffer.getValue() > 0 && offset > m_maxJitterBuffer.getValue())
		offset = m_maxJitterBuffer.getValue();

	if (m_pSSRCInfo->getNumberOfInsertTimes() == MIPRTPDECODER_HISTLEN) // need several packets for a good estimate
	{
		if (offset < MINOFFSET) // need more buffering
		{
			MIPTime curTime = MIPTime::getCurrentTime();
	
			if ((curTime.getValue() - m_pSSRCInfo->getLastOffsetAdjustTime().getValue()) > 0.200)
			{
				// we'll use 5 msec boundary
		
				real_t diff = MINOFFSET - offset;
				int64_t diffi = ((int64_t)((diff*200.0)+0.5));
	
				if (diffi > 0)
				{
					diff = ((real_t)diffi)/200.0;
	
					MIPTime newOffset = m_pSSRCInfo->getPlaybackOffset();
					newOffset += MIPTime(diff);
	
	//	std::cerr << "Insert average:  " << m_pSSRCInfo->getInsertTimeAverage().getString() << std::endl;
	//	std::cerr << "Insert variance: " << m_pSSRCInfo->getInsertTimeVariance().getString() << std::endl;
	//	std::cerr << "Jitter:          " << jitterValue.getString() << std::endl;
		
					m_pSSRCInfo->setPlaybackOffset(newOffset);
	//				std::cerr << "Increasing playback offset by " << MIPTime(diff).getString() << std::endl;
	//				std::cerr << "New offset is " << newOffset.getString() << std::endl;
				}
			}
		}
		else // perhaps we kan decrease the buffering somewhat, but we'll only make gradual adjustments every 5 seconds
		{
			MIPTime curTime = MIPTime::getCurrentTime();
			real_t delay;
			real_t ins = insertDiff;
	
			if (ins < 0)
				ins = 0;
	
			delay = 500.0/(1.0+100.0*ins);
			if (delay < 5.0)
				delay = 5.0;
			else if (delay > 60.0)
				delay = 60.0;
	
			if ((curTime.getValue() - m_pSSRCInfo->getLastOffsetAdjustTime().getValue()) > delay)
			{
//				std::cout << "Delay: " << MIPTime(delay).getString() << std::endl;
				
				// we'll use 5 msec boundary
		
				real_t diff = offset - MINOFFSET;
				
				if (diff > 0.005)
				{
					real_t diff2 = ((real_t)((int64_t)((diff*200.0)+0.5)))/200.0;
		
					if (diff2 > diff)
						diff2 -= 0.005;
					
					MIPTime newOffset = m_pSSRCInfo->getPlaybackOffset();
					newOffset -= MIPTime(diff2);
	
	//	std::cerr << "Insert average:  " << m_pSSRCInfo->getInsertTimeAverage().getString() << std::endl;
	//	std::cerr << "Insert variance: " << m_pSSRCInfo->getInsertTimeVariance().getString() << std::endl;
	//	std::cerr << "Jitter:          " << jitterValue.getString() << std::endl;
		
					m_pSSRCInfo->setPlaybackOffset(newOffset);
	//				std::cerr << "Decreasing playback offset by " << MIPTime(diff2).getString() << std::endl;
	//				std::cerr << "New offset is " << newOffset.getString() << std::endl;
				}
				else
				{
					// reinstall old offset so the adjustment time is initialized again
					MIPTime oldOffset = m_pSSRCInfo->getPlaybackOffset();
					m_pSSRCInfo->setPlaybackOffset(oldOffset);
				}
			}
		}
	}
	
	MIPTime x = streamTime;
	x -= m_playbackOffset;
	insertOffset = x;

	return true;
}

uint64_t MIPRTPDecoder::SSRCInfo::getExtendedTimestamp(uint32_t ts)
{
	if (ts == m_prevTimestamp)
		return m_cycles | ((uint64_t)ts);
	if (ts > m_prevTimestamp)
	{
		if ((ts - m_prevTimestamp) < 0x10000000)
		{
			m_prevTimestamp = ts;
			return m_cycles | (uint64_t)ts;
		}
		
		if ((m_prevTimestamp - ts) < 0x10000000)
		{
			if (m_cycles == 0) // can't handle this, reset
			{
				reset(ts);
				return m_baseTimestamp;
			}

			uint64_t cycles = m_cycles - (((uint64_t)1) << 32);
			uint64_t extTs = cycles | (uint64_t)ts;

			// note, here we don't adjust the previous timestamp (this would mean
			// readjusting the cycles too

			if (extTs < m_baseTimestamp) // can't handle this
			{
				reset(ts);
				return m_baseTimestamp;
			}
			return extTs;
		}
		
		// timestamp way out of range, reset
		
		reset(ts);
		return m_baseTimestamp;
	}

	// at this point, we're certain that ts < m_prevTimestamp

	if ((ts - m_prevTimestamp) < 0x10000000) // cycled
	{
		m_cycles += (((uint64_t)1) << 32);
		m_prevTimestamp = ts;
		return m_cycles | (uint64_t)ts;
	}

	if ((m_prevTimestamp - ts) < 0x10000000)
	{
		uint64_t extTs = m_cycles | (uint64_t)ts;

		// note, here we don't adjust the previous timestamp

		if (extTs < m_baseTimestamp) // can't handle this
		{
			reset(ts);
			return m_baseTimestamp;
		}
		return extTs;
	}
	
	// timestamp way out of range, reset
		
	reset(ts);
	return m_baseTimestamp;
}

