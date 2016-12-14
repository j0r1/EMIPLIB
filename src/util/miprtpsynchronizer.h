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
 * \file miprtpsynchronizer.h
 */

#ifndef MIPRTPSYNCHRONIZER_H

#define MIPRTPSYNCHRONIZER_H

#include "mipconfig.h"
#include "miperrorbase.h"
#include "miptime.h"
#include <jmutex.h>
#include <list>
#include <map>

//#include <iostream>

/** An object of this type can be used to synchronize several RTP streams from the same source.
 *  Using this object, you can synchronize several RTP streams of the same source. To do this,
 *  you simply have to pass a MIPRTPSynchronizer instance as an argument to the MIPRTPDecoder::init
 *  function of a MIPRTPDecoder derived class. Based upon the RTCP CNAME information, streams
 *  will be grouped and synchronized.
 */
class MIPRTPSynchronizer : public MIPErrorBase
{
public:
	MIPRTPSynchronizer();
	~MIPRTPSynchronizer();

	/** Locks the MIPRTPSynchronizer instance.
	 *  This function locks the MIPRTPSynchronizer instance. This function is useful since
	 *  the object can be accessed from different chains, each having their own background
	 *  thread.
	 */
	void lock()										{ m_mutex.Lock(); }

	/** Unlocks the MIPRTPSynchronizer instance. */
	void unlock()										{ m_mutex.Unlock(); }

	/** Resets the MIPRTPSynchronizer instance to its original state. */
	void clear();
	
	/** Sets the maximum amount of de-synchronization which may exist between streams of the
	 *  same source (default: 100 milliseconds).
	 */
	void setTolerance(MIPTime t)								{ m_tolerance = t; }
	
	bool registerStream(const uint8_t *pCName, size_t cnameLength, real_t timestampUnit, int64_t *streamID);
	bool unregisterStream(int64_t streamID);
	bool setStreamInfo(int64_t streamID, MIPTime SRwallclock, uint32_t SRtimestamp, uint32_t curTimestamp,
			   MIPTime outputStreamOffset, MIPTime totalComponentDelay);
	MIPTime calculateSynchronizationOffset(int64_t streamID);
private:
	class CNameInfo
	{
	public:
		CNameInfo(uint8_t *pCName = 0, size_t cnameLength = 0)				{ m_pCName = pCName; m_cnameLength = cnameLength; }
		~CNameInfo()									{ }
		uint8_t *getCName() const							{ return m_pCName; }
		size_t getCNameLength() const							{ return m_cnameLength; }
		
		bool operator()(CNameInfo c1, CNameInfo c2) const
		{
			if (c1.getCNameLength() < c2.getCNameLength())
				return true;
			if (c1.getCNameLength() > c2.getCNameLength())
				return false;
			if (memcmp(c1.getCName(), c2.getCName(), c1.getCNameLength()) >= 0)
				return false;
			return true;
		}
	private:
		uint8_t *m_pCName;
		size_t m_cnameLength;
	};
	
	class StreamGroup;
	
	class StreamInfo
	{
	public:
		StreamInfo(StreamGroup *pGroup, real_t tsUnit)					{ m_pGroup = pGroup; m_infoSet = false; m_tsUnit = tsUnit; }
		~StreamInfo()									{ }
		StreamGroup *getGroup()								{ return m_pGroup; }
		bool isInfoSet() const								{ return m_infoSet; }
		void setInfo(MIPTime SRwallclock, uint32_t SRtimestamp, uint32_t curTimestamp,
			     MIPTime outputStreamOffset, MIPTime totalComponentDelay)		
		{
			m_infoSet = true;
			m_lastInfoUpdateTime = MIPTime::getCurrentTime();
			m_SRwallclock = SRwallclock;
			m_SRtimestamp = SRtimestamp;
			m_outputStreamOffset = outputStreamOffset;
			m_totalComponentDelay = totalComponentDelay;
			m_lastTimestamp = curTimestamp;
		}
		MIPTime getLastUpdateTime() const						{ return m_lastInfoUpdateTime; }
		uint32_t getLastTimestamp() const						{ return m_lastTimestamp; }
		MIPTime getSRWallclockTime() const						{ return m_SRwallclock; }
		uint32_t getSRTimestamp() const							{ return m_SRtimestamp; }
		MIPTime getOutputStreamOffset() const						{ return m_outputStreamOffset; }
		MIPTime getTotalComponentDelay() const						{ return m_totalComponentDelay; }
		real_t getTimestampUnit() const							{ return m_tsUnit; }

		MIPTime getSynchronizationOffset() const					{ return m_syncOffset; }
		
		void calculateRemoteWallclockTime(MIPTime refTime)
		{
			MIPTime lastTimeDiff = refTime;
			lastTimeDiff -= m_lastInfoUpdateTime;

//			std::cout << std::endl;
//			std::cout << "lastTimeDiff: " << lastTimeDiff.getString() << std::endl;

			int32_t tsDiff;
			
			if ((m_lastTimestamp - m_SRtimestamp) < 0x80000000)
				tsDiff = (int32_t)(m_lastTimestamp - m_SRtimestamp);
			else
				tsDiff = -(int32_t)(m_SRtimestamp - m_lastTimestamp);

			MIPTime tsTimeDiff = MIPTime(((real_t)tsDiff)*m_tsUnit);

//			std::cout << "tsTimeDiff: " << tsTimeDiff.getString() << std::endl;

			MIPTime wallclock = m_SRwallclock;

//			std::cout << "m_SRwallclock: " << wallclock.getString() << std::endl;
			
			wallclock += tsTimeDiff;
			wallclock += lastTimeDiff;

			// 'wallclock' would be our result if there would be no component delay, output stream delay etc
			// Because of this, a sample played at 'refTime' will actually be older (an earlier timestamp)

//			std::cout << "outputStreamOffset: " << m_outputStreamOffset.getString() << std::endl;
//			std::cout << "totalComponentDelay: " << m_totalComponentDelay.getString() << std::endl;
//			std::cout << "syncOffset: " << m_syncOffset.getString() << std::endl;
			
			wallclock -= m_outputStreamOffset;
			wallclock -= m_totalComponentDelay;
			wallclock -= m_syncOffset;

//			std::cout << "Remote wallclock: " << wallclock.getString() << std::endl << std::endl;
			
			m_remoteWallclockTime = wallclock;
		}
		MIPTime getRemoteWallclockTime() const						{ return m_remoteWallclockTime; }
		void setOffsetAdjustment(MIPTime a)						{ m_adjustment = a; }
		void acceptAdjustment()								{ m_syncOffset += m_adjustment; }
		void adjustOffset(MIPTime t)							{ m_syncOffset -= t; }
	private:
		StreamGroup *m_pGroup;
		bool m_infoSet;
		MIPTime m_lastInfoUpdateTime;
		MIPTime m_SRwallclock;
		MIPTime m_outputStreamOffset;
		MIPTime m_totalComponentDelay;
		uint32_t m_SRtimestamp, m_lastTimestamp;
		real_t m_tsUnit;

		MIPTime m_syncOffset;
		MIPTime m_adjustment;
		MIPTime m_remoteWallclockTime;
	};

	class StreamGroup
	{
	public:
		StreamGroup()									{ }
		~StreamGroup() 									{ }
		std::list<StreamInfo *> &streams()						{ return m_streams; }
		void setStreamsChanged(bool f)							{ m_streamsChanged = f; }
		bool didStreamsChange() const							{ return m_streamsChanged; }
		MIPTime getLastCalculationTime() const						{ return m_lastCalcTime; }
		MIPTime calculateOffsets()
		{
			MIPTime referenceTime = MIPTime::getCurrentTime();
			m_lastCalcTime = referenceTime;
			
			std::list<StreamInfo *>::iterator it;
			MIPTime maxRemoteWallclockTime, minRemoteWallclockTime;	
			bool extSet = false;
			int i = 0;
			
			for (it = m_streams.begin() ; it != m_streams.end() ; it++, i++)
			{
				if ((*it)->isInfoSet())
				{
					//std::cout << "Calculating " << i << std::endl;
					(*it)->calculateRemoteWallclockTime(referenceTime);
					
					MIPTime t = (*it)->getRemoteWallclockTime();

					if (!extSet)
					{
						extSet = true;
						maxRemoteWallclockTime = t;
						minRemoteWallclockTime = t;
					}
					else
					{
						if (t > maxRemoteWallclockTime)
							maxRemoteWallclockTime = t;
						if (t < minRemoteWallclockTime)
							minRemoteWallclockTime = t;
					}
				}
		//		else
		//			std::cout << "Not calculating " << i << std::endl;
			}

			MIPTime diff = maxRemoteWallclockTime;
			diff -= minRemoteWallclockTime;

			for (it = m_streams.begin() ; it != m_streams.end() ; it++)
			{
				if ((*it)->isInfoSet())
				{
					MIPTime adjustment = (*it)->getRemoteWallclockTime();
					adjustment -= minRemoteWallclockTime;
					(*it)->setOffsetAdjustment(adjustment);
				}
			}
			
			return diff;
		}
		
		void acceptNewOffsets()
		{
			std::list<StreamInfo *>::iterator it;
			MIPTime minOffset;
			bool extSet = false;

			for (it = m_streams.begin() ; it != m_streams.end() ; it++)
			{
				if ((*it)->isInfoSet())
				{
					(*it)->acceptAdjustment();
					MIPTime t2 = (*it)->getSynchronizationOffset();
					
					if (!extSet)
					{
						extSet = true;
						minOffset = t2;
					}
					else
					{
						if (t2 < minOffset)
							minOffset = t2;
					}
				}
			}

			for (it = m_streams.begin() ; it != m_streams.end() ; it++)
			{
				if ((*it)->isInfoSet())
					(*it)->adjustOffset(minOffset);
			}
		}
	private:
		std::list<StreamInfo *> m_streams;
		bool m_streamsChanged;
		MIPTime m_lastCalcTime;
	};

	std::map<CNameInfo, StreamGroup *, CNameInfo> m_cnameTable;
	std::map<int64_t, StreamInfo *> m_streamIDTable;
	
	MIPTime m_tolerance;
	int64_t m_nextStreamID;
	JMutex m_mutex;
};

#endif // MIPRTPSYNCHRONIZER_H

