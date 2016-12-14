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

/**
 * \file mipv4l2input.h
 */

#ifndef MIPV4L2INPUT_H

#define MIPV4L2INPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_VIDEO4LINUX

#include "mipcomponent.h"
#include "mipsignalwaiter.h"
#include "miptime.h"
#include <jthread/jthread.h>

class MIPVideoMessage;

/** This class describes a Video4Linux2 based input component.
 *  This class describes a Video4Linux2 based input component. The component accepts both
 *  MIPSYSTEMMESSAGE_TYPE_WAITTIME and MIPSYSTEMMESSAGE_TYPE_ISTIME messages. Currently there
 *  is no way to specify the desired frame rate, so the MIPSYSTEMMESSAGE_TYPE_ISTIME is the
 *  recommended message to use. Output frames can be YUV420P encoded, YUYV encoded or JPEG
 *  compressed.
 */
class MIPV4L2Input : public MIPComponent, private jthread::JThread
{
public:
	MIPV4L2Input();
	~MIPV4L2Input();

	/** Opens the specified video4linux2 device and requests a specific width and height. */
	bool open(int width, int height, const std::string &device = std::string("/dev/video0"));

	/** Closes the video4linux2 device. */
	bool close();

	/** Returns the width of the captured video frames. */
	int getWidth() const									{ if (m_device != -1) return m_width; return -1; }

	/** Returns the height of the captured video frames. */
	int getHeight() const									{ if (m_device != -1) return m_height; return -1; }
	
	/** Sets the source ID to be stored in generated messages. */
	void setSourceID(uint64_t srcID)							{ m_sourceID = srcID; }

	/** Returns the source ID which will be stored in generated messages. */
	uint64_t getSourceID() const								{ return m_sourceID; }

	/** Returns true is the frames are JPEG compressed, false if not. */
	bool getCompressed() const								{ return m_compressed; }
	
	/** Returns the message subtype of the frames produced by this component (for uncompressed frames). */
	uint32_t getFrameSubtype() const							{ return m_subType; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void *Thread();
	void cleanUp();

	int m_device;
	int m_width, m_height;
	bool m_streaming, m_userPtr;
	bool m_compressed;
	uint8_t *m_pCurFrame;
	uint8_t *m_pFullFrame;
	uint8_t *m_pMsgFrame;
	size_t m_frameSize;
	MIPVideoMessage *m_pVideoMsg;
	jthread::JMutex m_frameMutex;
	jthread::JMutex m_stopMutex;
	MIPSignalWaiter m_sigWait;
	bool m_gotMsg, m_stopLoop;
	bool m_gotFrame;
	MIPTime m_captureTime;
	uint64_t m_sourceID;
	uint32_t m_subType;
};

#endif // MIPCONFIG_SUPPORT_VIDEO4LINUX

#endif // MIPV4L2INPUT_H

