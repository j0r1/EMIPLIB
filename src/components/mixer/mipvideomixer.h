/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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
 * \file mipvideomixer.h
 */

#ifndef MIPVIDEOMIXER_H

#define MIPVIDEOMIXER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "miprawvideomessage.h"
#include <list>

/** This component creates video output streams.
 *  The name 'mixer' is a bit misleading for this component since it does not
 *  actually mix several video frames into one frame. However, it does work in
 *  a very similar way as the audio mixer, which is the reason for its name.
 *  The component accepts raw video messages in YUV420P format and stores them
 *  in an output queue based on the timing information contained in the messages.
 *  During each interval, a number of raw video frames in YUV420P format is 
 *  produced.
 */
class EMIPLIB_IMPORTEXPORT MIPVideoMixer : public MIPComponent
{
public:
	MIPVideoMixer();
	~MIPVideoMixer();

	/** Initializes the component.
	 *  Using this function, the component can be initialized.
	 *  \param frameRate The output frame rate.
	 *  \param maxStreams Maximum allowed number of streams. If set to -1, the number
	 *                    of streams is unlimited.
	 */
	bool init(real_t frameRate, int maxStreams = -1);

	/** De-initializes the component. */
	bool destroy();

	/** Adds an additional playback delay.
	 *  Using this function, an additional playback delay can be introduced. Note that only
	 *  positive delays are allowed. This can be useful for inter-media synchronization, in
	 *  case not all the component delays are known well enough to provide exact synchronization.
	 *  Using this function, the synchronization can then be adjusted manually.
	 */
	bool setExtraDelay(MIPTime t);
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
	bool processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback);
private:
	class VideoFrame
	{
	public:
		VideoFrame(int64_t playInterval, MIPRawYUV420PVideoMessage *pMsg)		{ m_interval = playInterval; m_pMsg = pMsg; }
		MIPRawYUV420PVideoMessage *getMessage()						{ return m_pMsg; }
		int64_t getInterval() const							{ return m_interval; }
	private:
		MIPRawYUV420PVideoMessage *m_pMsg;
		int64_t m_interval;
	};

	class SourceStream
	{
	public:
		SourceStream(uint64_t sourceID)							{ m_sourceID = sourceID; }
		~SourceStream()									{ std::list<VideoFrame>::iterator it; for (it = m_videoFrames.begin() ; it != m_videoFrames.end(); it++) if ((*it).getMessage()) delete (*it).getMessage(); }
		uint64_t getSourceID() const							{ return m_sourceID; }
		std::list<VideoFrame> &getFrames()						{ return m_videoFrames; }
		MIPTime getLastInsertTime() const						{ return m_lastInsertTime; }
		MIPRawYUV420PVideoMessage *ExtractMessage(int64_t curInterval)
		{
			if (m_videoFrames.empty())
				return 0;

			VideoFrame frame = *(m_videoFrames.begin());
			if (frame.getInterval() == curInterval)
			{
				m_videoFrames.pop_front();
				return frame.getMessage();
			}
			return 0;
		}
		void insertFrame(int64_t frameNum, MIPRawYUV420PVideoMessage *pMsg)
		{
			m_lastInsertTime = MIPTime::getCurrentTime();

			std::list<VideoFrame>::iterator it;

			it = m_videoFrames.begin();
			
			if (it == m_videoFrames.end())
			{
				m_videoFrames.push_back(VideoFrame(frameNum,pMsg));
				return;
			}

			if (frameNum < (*it).getInterval())
			{
				m_videoFrames.insert(it, VideoFrame(frameNum,pMsg));
				return;
			}

			if (frameNum == (*it).getInterval())
			{
				// already a frame present, replace it with the new frame
				delete (*it).getMessage();
				(*it) = VideoFrame(frameNum,pMsg);
				return;
			}

			// frameNum > (*it).getInterval()
			
			bool done = false;
	
			while (!done)
			{
				it++;
				if (it == m_videoFrames.end())
				{
					done = true;
				}
				else
				{
					if ((*it).getInterval() == frameNum)
					{
						// already a frame present, replace it with the new frame
						delete (*it).getMessage();
						(*it) = VideoFrame(frameNum,pMsg);
						return;
					}

					if (frameNum < (*it).getInterval())
					{
						m_videoFrames.insert(it, VideoFrame(frameNum,pMsg));
						return;
					}
				}
			} 
	
			m_videoFrames.push_back(VideoFrame(frameNum,pMsg));
		}
	private:
		uint64_t m_sourceID;
		std::list<VideoFrame> m_videoFrames;
		MIPTime m_lastInsertTime;
	};

	bool initFrameSearch(uint64_t sourceID);
	void clearOutputMessages();
	void createNewOutputMessages();
	void deleteOldSources();
	
	bool m_init;
	int m_maxStreams;
	int64_t m_prevIteration;
	int64_t m_curInterval;
	MIPTime m_playTime, m_frameTime, m_lastCheckTime;
	MIPTime m_extraDelay;
	std::list<SourceStream *> m_streams;
	std::list<MIPRawYUV420PVideoMessage *> m_outputMessages;
	std::list<MIPRawYUV420PVideoMessage *>::const_iterator m_msgIt;
};

#endif // MIPVIDEOMIXER_H

