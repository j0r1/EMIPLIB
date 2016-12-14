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

/**
 * \file miprtpdecoder.h
 */

#ifndef MIPRTPDECODER_H

#define MIPRTPDECODER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#if defined(WIN32) || defined(_WIN32_WCE)
	#include <hash_map>
#else
	#include <ext/hash_map>
#endif // Win32
#include <cmath>
#include <list>

class RTPPacket;
class RTPSession;
class MIPRTPReceiveMessage;
class MIPRTPSynchronizer;
class MIPMediaMessage;
class MIPRTPPacketDecoder;

#define MIPRTPDECODER_MAXPAYLOADDECODERS							256

/** A base class for RTP decoding objects.
 *  This class provides some general functions for decoding RTP packets. It analyzes
 *  The RTP timestamps, recreates the output streams and introduces some jitter
 *  buffering if necessary. It requires feedback about some kind of output stream,
 *  like the one the MIPAudioMixer component generates. The component accepts
 *  MIPRTPReceiveMessage objects; a derived class must implement the message
 *  generation code, therefore it is the derived class that decides which messages 
 *  can be generated.
 */
class MIPRTPDecoder : public MIPComponent
{
public:
	MIPRTPDecoder();
	~MIPRTPDecoder();

	/** Initializes the RTP decoder.
	 *  Using this function, the RTP decoder can be initialized.
	 *  \param calcStreamTime If set to \c true, the decoder will calculate the position of
	 *                        the received packets in the output stream. For this feature
	 *                        to work, the decoder must receive feedback from a mixer.
	 *  \param pSynchronizer If a non-null pointer is passed, the object will be used
	 *                       to synchronize different streams from the same participant.
	 *  \param pRTPSess If specified, the timestamp unit returned by an MIPRTPPacketDecoder instance
	 *                  will be stored in the RTPSourceData instance of the corresponding SSRC.
	 */
	bool init(bool calcStreamTime = true, MIPRTPSynchronizer *pSynchronizer = 0, RTPSession *pRTPSess = 0);

	/** Installs a default RTP packet decoder (for all payload types).
	 *  Installs a default RTP packet decoder. Use a null pointer to clear all entries.
	 */
	bool setDefaultPacketDecoder(MIPRTPPacketDecoder *pDec);

	/** Installs an RTP packet decoder for a specific payload type. 
	 *  Installs an RTP packet decoder for a specific payload type. Use a null pointer to
	 *  clear this particular entry.
	 */
	bool setPacketDecoder(uint8_t payloadType, MIPRTPPacketDecoder *pDec);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	bool processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback);
private:
	void clearMessages();
	void cleanUp();
	void cleanUpSourceTable();
	bool lookUpStreamTime(const MIPRTPReceiveMessage *pRTPPack, real_t timestampUnit, MIPTime &streamTime, bool &shouldSync);
	bool adjustToPlaybackTime(const MIPRTPReceiveMessage *pRTPMsg, MIPTime &streamTime, MIPTime &insertOffset);

	bool m_init;	
	int64_t m_prevIteration;
	std::list<MIPMediaMessage *> m_messages;
	std::list<MIPMediaMessage *>::const_iterator m_msgIt;

	bool m_gotPlaybackFeedback;
	MIPTime m_playbackOffset;

	class SSRCInfo
	{
	public:
		SSRCInfo(uint32_t baseTimestamp = 0) : m_lastAccessTime(0), m_playbackOffset(0), m_avgInsertTime(0),
							m_insertTimeSpread(0), m_lastOffsetAdjustTime(0), m_lastSyncTime(0), m_syncOffset(0)
									{ reset(baseTimestamp); m_syncStreamID = -1; }
		void setSyncStreamID(int64_t id)			{ m_syncStreamID = id; }
		int64_t getSyncStreamID() const				{ return m_syncStreamID; }
		MIPTime getLastSyncTime() const				{ return m_lastSyncTime; }
		void setLastSyncTime(MIPTime t)				{ m_lastSyncTime = t; }
		MIPTime getSyncOffset() const				{ return m_syncOffset; }
		void setSyncOffset(MIPTime t)				{ m_syncOffset = t; }

		MIPTime getLastAccessTime() const			{ return m_lastAccessTime; }
		uint64_t getBaseTimestamp() const			{ return m_baseTimestamp; }
		uint64_t getExtendedTimestamp(uint32_t ts);
		void setLastAccessTime(MIPTime t)			{ m_lastAccessTime = t; }
		
		MIPTime getPlaybackOffset() const			{ return m_playbackOffset; }
		MIPTime getInsertTimeAverage() const			{ return m_avgInsertTime; }
		MIPTime getInsertTimeVariance() const			{ return m_insertTimeSpread; }
		MIPTime getLastOffsetAdjustTime() const			{ return m_lastOffsetAdjustTime; }
		bool hasLastOffsetAdjustTime() const			{ return m_gotPlaybackOffset; }
		int64_t getNumberOfRecentPacket() const			{ return m_recentPacksReceived; }
		
		int getNumberOfInsertTimes() const			{ return m_numInsertTimes; }
			
		void setPlaybackOffset(MIPTime offset)			{ clearAdjustmentInfo(); m_playbackOffset = offset; m_gotPlaybackOffset = true; m_lastOffsetAdjustTime = MIPTime::getCurrentTime(); }
		void addInsertTime(MIPTime t)
		{
#define MIPRTPDECODER_HISTLEN 16
			m_insertTimes[m_curInsertTimePos] = t;
			m_curInsertTimePos++;
			if (m_curInsertTimePos == MIPRTPDECODER_HISTLEN)
				m_curInsertTimePos = 0;
			if (m_numInsertTimes < MIPRTPDECODER_HISTLEN)
				m_numInsertTimes++;

			real_t avg = 0;
			real_t sigma2 = 0;
			for (int i = 0 ; i < m_numInsertTimes ; i++)
				avg += m_insertTimes[i].getValue();
			avg /= (real_t)m_numInsertTimes;

			for (int i = 0 ; i < m_numInsertTimes ; i++)
			{
				real_t diff = m_insertTimes[i].getValue() - avg;
				real_t diff2 = diff*diff;
				sigma2 += diff2;
			}
			sigma2 /= (real_t)m_numInsertTimes;
			real_t sigma = (real_t)sqrt(sigma2);

			m_avgInsertTime = MIPTime(avg);
			m_insertTimeSpread = MIPTime(sigma);
		}

		void clearAdjustmentInfo()
		{
			m_playbackOffset = MIPTime(0);
			m_avgInsertTime = MIPTime(0);
			m_insertTimeSpread = MIPTime(0);
			m_lastOffsetAdjustTime = MIPTime(0);
			m_gotPlaybackOffset = false;
			m_recentPacksReceived = 0;
			m_numInsertTimes = 0;
			m_curInsertTimePos = 0;
		}
		
	private:
		void reset(uint32_t baseTimestamp)			
		{ 
			m_baseTimestamp = (uint64_t)baseTimestamp; 
			m_prevTimestamp = baseTimestamp; 
			m_cycles = 0; 
			m_lastAccessTime = MIPTime(0);
			clearAdjustmentInfo();
		}

		uint64_t m_cycles;
		uint64_t m_baseTimestamp;
		uint32_t m_prevTimestamp;
		MIPTime m_lastAccessTime;
		MIPTime m_playbackOffset;
		
		MIPTime m_avgInsertTime;
		MIPTime m_insertTimeSpread;
		MIPTime m_insertTimes[MIPRTPDECODER_HISTLEN];
		int m_numInsertTimes;
		int m_curInsertTimePos;
		
		MIPTime m_lastOffsetAdjustTime;
		bool m_gotPlaybackOffset;
		int64_t m_recentPacksReceived;

		int64_t m_syncStreamID;
		MIPTime m_lastSyncTime;
		MIPTime m_syncOffset;
	};

#if defined(WIN32) || defined(_WIN32_WCE)
	stdext::hash_map<uint32_t, SSRCInfo> m_sourceTable;
#else
	__gnu_cxx::hash_map<uint32_t, SSRCInfo> m_sourceTable;
#endif // Win32

	MIPRTPPacketDecoder *m_pDecoders[MIPRTPDECODER_MAXPAYLOADDECODERS];

	MIPTime m_prevCleanTableTime;
	SSRCInfo *m_pSSRCInfo;
	bool m_calcStreamTime;
	MIPRTPSynchronizer *m_pSynchronizer;
	RTPSession *m_pRTPSess;
	MIPTime m_totalComponentDelay;
};

#endif // MIPRTPDECODER_H

