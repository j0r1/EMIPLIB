/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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
#include "mipmediabuffer.h"
#include "mipmediamessage.h"
#include "mipfeedback.h"

#include "mipdebug.h"

#define MIPMEDIABUFFER_ERRSTR_ALREADYINIT				"Already initialized"
#define MIPMEDIABUFFER_ERRSTR_NOTINIT					"Not initialized"
#define MIPMEDIABUFFER_ERRSTR_BADMESSAGE				"Bad message"
#define MIPMEDIABUFFER_ERRSTR_CANTCREATECOPY				"Unable to create a copy of a message"

MIPMediaBuffer::MIPMediaBuffer() : MIPComponent("MIPMediaBuffer")
{
	m_init = false;
}

MIPMediaBuffer::~MIPMediaBuffer()
{
	destroy();
}

bool MIPMediaBuffer::init(MIPTime interval)
{
	if (m_init)
	{
		setErrorString(MIPMEDIABUFFER_ERRSTR_ALREADYINIT);
		return false;
	}

	m_interval = interval;
	m_gotPlaybackTime = false;
	m_prevIteration = -1;
	m_msgIt = m_messages.begin();
	m_init = true;
	
	return true;
}

bool MIPMediaBuffer::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPMEDIABUFFER_ERRSTR_NOTINIT);
		return false;
	}
	
	clearMessages();
	clearBuffers();
	m_init = false;
	
	return true;
}

bool MIPMediaBuffer::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPMEDIABUFFER_ERRSTR_NOTINIT);
		return false;
	}

	uint32_t msgType = pMsg->getMessageType();

	if (!(msgType == MIPMESSAGE_TYPE_AUDIO_RAW || msgType == MIPMESSAGE_TYPE_AUDIO_ENCODED ||
	      msgType == MIPMESSAGE_TYPE_VIDEO_RAW || msgType == MIPMESSAGE_TYPE_VIDEO_ENCODED) )
	{
		setErrorString(MIPMEDIABUFFER_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (!m_gotPlaybackTime)
		return true; // ignore messages until we've received feedback about some mixer
	
	MIPMediaMessage *pMediaMsg = (MIPMediaMessage *)pMsg;
	MIPMediaMessage *pNewMsg = pMediaMsg->createCopy();

	if (pNewMsg == 0)
	{
		setErrorString(MIPMEDIABUFFER_ERRSTR_CANTCREATECOPY);
		return false;
	}
	
	m_buffers.push_back(pNewMsg);

	return true;
}

bool MIPMediaBuffer::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPMEDIABUFFER_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_prevIteration)
	{
		m_prevIteration = iteration;
		clearMessages();
		buildOutputMessages();
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

bool MIPMediaBuffer::processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback)
{
	if (!m_init)
	{
		setErrorString(MIPMEDIABUFFER_ERRSTR_NOTINIT);
		return false;
	}	
	
	if (feedback->hasPlaybackStreamTime())
	{
		m_gotPlaybackTime = true;
		m_playbackTime = feedback->getPlaybackStreamTime();
	}
	
	return true;
}

void MIPMediaBuffer::clearMessages()
{
	std::list<MIPMediaMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

void MIPMediaBuffer::clearBuffers()
{
	std::list<MIPMediaMessage *>::iterator it;

	for (it = m_buffers.begin() ; it != m_buffers.end() ; it++)
		delete (*it);
	m_buffers.clear();
}

void MIPMediaBuffer::buildOutputMessages()
{
	if (m_gotPlaybackTime)
	{	
		std::list<MIPMediaMessage *>::iterator it;
	
		MIPTime compareTime = m_playbackTime;
		compareTime += m_interval;
		
		it = m_buffers.begin();
		while (it != m_buffers.end())
		{
			MIPTime t = (*it)->getTime();
			
			if (t < compareTime)
			{
				insertInOutput(*it);
				it = m_buffers.erase(it);
			}
			else
				it++;
		}
	}	
	
	m_msgIt = m_messages.begin();
}

void MIPMediaBuffer::insertInOutput(MIPMediaMessage *pMsg)
{
	std::list<MIPMediaMessage *>::iterator it;
	MIPTime t = pMsg->getTime();
	
	it = m_messages.begin();
	while (it != m_messages.end())
	{
		if (t < (*it)->getTime())
		{
			m_messages.insert(it, pMsg);
			return;
		}
		it++;
	}
	m_messages.push_back(pMsg);
}

