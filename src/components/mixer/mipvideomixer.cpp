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
#include "mipvideomixer.h"
#include "mipfeedback.h"

#include "mipdebug.h"

#define MIPVIDEOMIXER_ERRSTR_NOTINIT					"Video mixer not initialized"
#define MIPVIDEOMIXER_ERRSTR_ALREADYINIT				"Video mixer already initialized"
#define MIPVIDEOMIXER_ERRSTR_BADMESSAGE					"Bad message"
#define MIPVIDEOMIXER_ERRSTR_CANTSETPLAYBACKTIME			"Can't set playback time in feedback message"
#define MIPVIDEOMIXER_ERRSTR_DELAYTOOLARGE			"The specified extra delay is too large"
#define MIPVIDEOMIXER_ERRSTR_NEGATIVEDELAY			"Only positive delays are allowed"


MIPVideoMixer::MIPVideoMixer() : MIPComponent("MIPVideoMixer"), m_playTime(0), m_frameTime(0), m_lastCheckTime(0)
{
	m_init = false;
}

MIPVideoMixer::~MIPVideoMixer()
{
	destroy();
}

bool MIPVideoMixer::init(real_t frameRate, int maxStreams)
{
	if (m_init)
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_frameTime = MIPTime(1.0/frameRate);
	m_playTime = MIPTime(0);
	m_lastCheckTime = MIPTime::getCurrentTime();

	m_maxStreams = maxStreams;
	m_prevIteration = -1;
	m_curInterval = 0;
	m_extraDelay = MIPTime(0);

	m_msgIt = m_outputMessages.begin();
	m_init = true;	

	return true;
}

bool MIPVideoMixer::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_NOTINIT);
		return false;
	}
	
	clearOutputMessages();

	std::list<SourceStream *>::iterator it;
	
	for (it = m_streams.begin() ; it != m_streams.end() ; it++)
		delete *it;
	
	m_init = false;
	return true;
}

bool MIPVideoMixer::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_NOTINIT);
		return false;
	}	

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUV420P))
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	MIPRawYUV420PVideoMessage *pVidMsg = (MIPRawYUV420PVideoMessage *)pMsg;
	MIPTime msgTime = pVidMsg->getTime();

	if (msgTime < m_playTime) // frames are too old to be played. Ignore
		return true;

	MIPTime offsetTime = msgTime;
	offsetTime -= m_playTime;

	if (offsetTime.getValue() > 300.0) // if it's more than five minutes in the future, ignore it
		return true;

	offsetTime += m_extraDelay;

	// search for the right stream	
	
	int64_t frameNum = (int64_t)(offsetTime.getValue()/m_frameTime.getValue())+m_curInterval;
	std::list<SourceStream *>::iterator streamIt;
	bool found = false;
	uint64_t sourceID = pVidMsg->getSourceID();
	SourceStream *stream = 0;
	
	streamIt = m_streams.begin();
	while (!found && streamIt != m_streams.end())
	{
		if ((*streamIt)->getSourceID() == sourceID)
			found = true;
		else
			streamIt++;
	}

	if (!found) // check if we can add a stream
	{
		if (m_maxStreams > 0 && (int)m_streams.size() >= m_maxStreams) // maximum number of streams reached, ignore the packet
			return true;
		
		stream = new SourceStream(sourceID);
		m_streams.push_back(stream);
	}
	else
		stream = *streamIt;
	
	// create a copy of the message
	
	int width = pVidMsg->getWidth();
	int height = pVidMsg->getHeight();
	int dataSize = (width*height*3)/2;
	uint8_t *pNewFrameData = new uint8_t [dataSize];
	MIPRawYUV420PVideoMessage *pNewMsg = new MIPRawYUV420PVideoMessage(width, height, pNewFrameData, true);
	
	memcpy(pNewFrameData, pVidMsg->getImageData(), dataSize);
	pNewMsg->copyMediaInfoFrom(*pVidMsg);

	// insert it
	
	stream->insertFrame(frameNum,pNewMsg);
	
	return true;
}

bool MIPVideoMixer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_NOTINIT);
		return false;
	}	

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearOutputMessages();
		createNewOutputMessages();
		deleteOldSources();

		m_curInterval++;
		m_playTime += m_frameTime;
	}

	if (m_msgIt == m_outputMessages.end())
	{
		*pMsg = 0;
		m_msgIt = m_outputMessages.begin();
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}
	
	return true;
}

void MIPVideoMixer::clearOutputMessages()
{
	std::list<MIPRawYUV420PVideoMessage *>::iterator it;

	for (it = m_outputMessages.begin() ; it != m_outputMessages.end() ; it++)
		delete *it;
	m_outputMessages.clear();
	m_msgIt = m_outputMessages.begin();
}

void MIPVideoMixer::createNewOutputMessages()
{
	std::list<SourceStream *>::iterator it;

	for (it = m_streams.begin() ; it != m_streams.end() ; it++)
	{
		MIPRawYUV420PVideoMessage *pMsg = (*it)->ExtractMessage(m_curInterval);

		if (pMsg)
			m_outputMessages.push_back(pMsg);
	}
	m_msgIt = m_outputMessages.begin();
}

void MIPVideoMixer::deleteOldSources()
{
	MIPTime curTime = MIPTime::getCurrentTime();

	if ((curTime.getValue() - m_lastCheckTime.getValue()) < 5.0) // wait 5 seconds between checks
		return;
	
	m_lastCheckTime = curTime;
	
	std::list<SourceStream *>::iterator it;
	it = m_streams.begin();
	while (it != m_streams.end())
	{
		if ((curTime.getValue() - (*it)->getLastInsertTime().getValue()) > 30.0) // remove after 30 seconds of inactivity
			it = m_streams.erase(it);
		else
			it++;
	}
}

bool MIPVideoMixer::processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, 
                                    MIPFeedback *feedback)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_NOTINIT);
		return false;
	}
	if (!feedback->setPlaybackStreamTime(m_playTime))
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_CANTSETPLAYBACKTIME);
		return false;
	}
	
	feedback->addToPlaybackDelay(m_extraDelay);
	return true;
}

bool MIPVideoMixer::setExtraDelay(MIPTime t)
{
	if (t.getValue() < 0)
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_NEGATIVEDELAY);
		return false;
	}

	if (t.getValue() > 300.0)
	{
		setErrorString(MIPVIDEOMIXER_ERRSTR_DELAYTOOLARGE);
		return false;
	}
	
	m_extraDelay = t;
	return true;
}

