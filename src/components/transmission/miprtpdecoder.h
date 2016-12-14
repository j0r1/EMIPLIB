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
class MIPRTPReceiveMessage;
class MIPRTPSynchronizer;
class MIPMediaMessage;

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
protected:
	/** Constructor, meant to be used by subclasses. */
	MIPRTPDecoder(const std::string &componentName);
public:
	~MIPRTPDecoder();

	/** Initializes the RTP decoder.
	 *  Using this function, the RTP decoder can be initialized.
	 *  \param calcStreamTime If set to \c true, the decoder will calculate the position of
	 *                        the received packets in the output stream. For this feature
	 *                        to work, the decoder must receive feedback from a mixer.
	 *  \param pSynchronizer If a non-null pointer is passed, the object will be used
	 *                       to synchronize different streams from the same participant.
	 */
	bool init(bool calcStreamTime = true, MIPRTPSynchronizer *pSynchronizer = 0);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	bool processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback);
protected:
	
	/** Validates an RTP packet and gives information about the timestamp unit of the packet data.
	 *  This function validates an RTP packet and provides information about the timestamp unit
	 *  of the packet data. It has to be implemented in a derived class. The function should return 
	 *  true if the packet is valid and false otherwise and if possible, the timestamp unit should
	 *  be filled in.
	 */
	virtual bool validatePacket(const RTPPacket *pRTPPack, real_t &timestampUnit) = 0;

	/** Creates a new message from an RTP packet.
	 *  This function has to be implemented by a derived class. Based on the validated
	 *  RTP packet \c pRTPPack, an appropriate message should be generated. If something 
	 *  went wrong, the function should return 0.
	 */
	virtual MIPMediaMessage *createNewMessage(const RTPPacket *pRTPPack) = 0;
private:
	void clearMessages();
	void cleanUp();
	void cleanUpSourceTable();
	bool lookUpStreamTime(const MIPRTPReceiveMessage *pRTPPack, real_t timestampUnit, MIPTime &streamTime);
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
							m_insertTimeSpread(0), m_lastOffsetAdjustTime(0)
									{ reset(baseTimestamp); m_syncStreamID = -1; }
		void setSyncStreamID(int64_t id)			{ m_syncStreamID = id; }
		int64_t getSyncStreamID() const				{ return m_syncStreamID; }

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
	};

#if defined(WIN32) || defined(_WIN32_WCE)
	stdext::hash_map<uint32_t, SSRCInfo> m_sourceTable;
#else
	__gnu_cxx::hash_map<uint32_t, SSRCInfo> m_sourceTable;
#endif // Win32

	MIPTime m_prevCleanTableTime;
	SSRCInfo *m_pSSRCInfo;
	bool m_calcStreamTime;
	MIPRTPSynchronizer *m_pSynchronizer;
	MIPTime m_totalComponentDelay;
};

#endif // MIPRTPDECODER_H

