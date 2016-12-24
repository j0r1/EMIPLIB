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

#include "mipconfig.h"
#include "mipvideoframestorage.h"
#include "miprawvideomessage.h"
#include <string.h>

#include "mipdebug.h"

#define MIPVIDEOFRAMESTORAGE_ERRSTR_NOTINIT					"Not initialized"
#define MIPVIDEOFRAMESTORAGE_ERRSTR_ALREADYINIT					"Already initialized"
#define MIPVIDEOFRAMESTORAGE_ERRSTR_NOPULL					"Pull is not supported"
#define MIPVIDEOFRAMESTORAGE_ERRSTR_NOTFOUND					"No frame found for specified source"
#define MIPVIDEOFRAMESTORAGE_ERRSTR_BADMESSAGE					"Bad message"

MIPVideoFrameStorage::MIPVideoFrameStorage() : MIPComponent("MIPVideoFrameStorage")
{
	m_init = false;
}

MIPVideoFrameStorage::~MIPVideoFrameStorage()
{
	destroy();
}

bool MIPVideoFrameStorage::init()
{
	if (m_init)
	{
		setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_ALREADYINIT);
		return false;
	}
	m_lastExpireTime = MIPTime::getCurrentTime();
	m_init = true;
	return true;
}
	
bool MIPVideoFrameStorage::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_NOTINIT);
		return false;
	}

	std::map<uint64_t, VideoFrameInfo *>::iterator it;

	for (it = m_frames.begin() ; it != m_frames.end() ; it++)
		delete (*it).second;
	m_frames.clear();
	
	m_init = false;
	return true;
}

bool MIPVideoFrameStorage::getData(uint64_t sourceID, uint8_t *pDest, int *width, int *height, MIPTime *t)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_NOTINIT);
		return false;
	}	
	
	std::map<uint64_t, VideoFrameInfo *>::const_iterator it = m_frames.find(sourceID);

	if (it == m_frames.end())
	{
		setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_NOTFOUND);
		return false;
	}

	if (pDest)
		memcpy(pDest, (*it).second->getData(), (*it).second->getDataLength());
	if (width)
		*width = (*it).second->getWidth();
	if (height)
		*height = (*it).second->getHeight();
	if (t)
		*t = (*it).second->getCreationTime();
	
	return true;
}

bool MIPVideoFrameStorage::getSourceIDs(std::list<uint64_t> &sourceIDs)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_NOTINIT);
		return false;
	}	
	
	std::map<uint64_t, VideoFrameInfo *>::const_iterator it;
	
	sourceIDs.clear();
	for (it = m_frames.begin() ; it != m_frames.end() ; it++)
		sourceIDs.push_back((*it).first);
	return true;
}

bool MIPVideoFrameStorage::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_NOTINIT);
		return false;
	}
	
	expire();

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUV420P))
	{
		setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRawYUV420PVideoMessage *pVidMsg = (MIPRawYUV420PVideoMessage *)pMsg;
	
	int width = pVidMsg->getWidth();
	int height = pVidMsg->getHeight();
	size_t length = (size_t)((width*height*3)/2);
	uint8_t *pData = new uint8_t[length];
	uint64_t sourceID = pVidMsg->getSourceID();
	
	memcpy(pData, pVidMsg->getImageData(), length);
	VideoFrameInfo *pInf = new VideoFrameInfo(pData, width, height, length);

	std::map<uint64_t, VideoFrameInfo *>::iterator it = m_frames.find(sourceID);
	
	if (it == m_frames.end())
	{
		m_frames[sourceID] = pInf;
	}
	else
	{
		delete (*it).second;
		(*it).second = pInf;
	}
	
	return true;
}

bool MIPVideoFrameStorage::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPVIDEOFRAMESTORAGE_ERRSTR_NOPULL);
	return false;
}

void MIPVideoFrameStorage::expire()
{
	MIPTime curTime = MIPTime::getCurrentTime();

	if (curTime.getValue() - m_lastExpireTime.getValue() < 10.0)
		return;
	m_lastExpireTime = curTime;

	std::map<uint64_t, VideoFrameInfo *>::iterator it;
	it = m_frames.begin();
	while (it != m_frames.end())
	{
		VideoFrameInfo *pInf = (*it).second;

		if (curTime.getValue() - pInf->getCreationTime().getValue() > 10.0)
		{
			std::map<uint64_t, VideoFrameInfo *>::iterator it2 = it;
			it++;

			m_frames.erase(it2);
			delete pInf;
		}
		else
			it++;
	}
}
	
