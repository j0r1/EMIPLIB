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

/**
 * \file mipv4linput.h
 */

#ifndef MIPV4LINPUT_H

#define MIPV4LINPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_VIDEO4LINUX

#include "mipcomponent.h"
#include "mipsignalwaiter.h"
#include "miptime.h"
#include <jthread.h>

class MIPRawYUV420PVideoMessage;

/** Base class for video4linux input component parameters. */
class MIPV4LInputParameters
{
public:
	MIPV4LInputParameters()									{ }
	virtual ~MIPV4LInputParameters()							{ }
};

/** This class is a base class for video4linux based video input.
 *  This component is a basic video4linux input component. Specific components can be derived
 *  from this class and can be initialized by overriding the MIPV4LInput::subInit member function.
 *  The component accepts both MIPSYSTEMMESSAGE_TYPE_WAITTIME and MIPSYSTEMMESSAGE_TYPE_ISTIME 
 *  messages.
 */
class MIPV4LInput : public MIPComponent, private JThread
{
public:
	MIPV4LInput();
protected:
	/** Constructor meant to be used by subclasses. */
	MIPV4LInput(const std::string &compName);
public:
	~MIPV4LInput();

	/** Opens a video4linux device.
	 *  Using this function, a video4linux device can be opened.
	 *  \param device The name of the video4linux device.
	 *  \param pParams Additional parameters for the video4linux device. These can be used by
	 *                 a subclass if the MIPV4LInput::subInit member function is overridden.
	 */
	bool open(const std::string &device = std::string("/dev/video0"), const MIPV4LInputParameters *pParams = 0);

	/** Closes the video4linux device. */
	bool close();

	/** Returns the width of the captured video frames. */
	int getWidth() const									{ if (m_device != -1) return m_width; return -1; }

	/** Returns the height of the captured video frames. */
	int getHeight() const									{ if (m_device != -1) return m_height; return -1; }

	/** Sets the source ID to be stored in generated messages. */
	void setSourceID(uint64_t srcID)							{ m_sourceID = srcID; }

	/** Returns the source ID which will be stored in generated messages. */
	uint64_t getSourceID() const								{ return m_sourceID; }
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
protected:
	/** Virtual function, meant to be overridden by subclasses.
	 *  This function can be overridden by subclasses and can be used to perform device
	 *  specific initialization. In case of an error, the subclass should set the error
	 *  string itself.
	 *  \param device The device identifier.
	 *  \param pParams Device specific parameters. A subclass should probably also derive
	 *                 a class from MIPV4LInputParameters in which additional parameters
	 *                 can be specified.
	 */
	virtual bool subInit(int device, const MIPV4LInputParameters *pParams)			{ return true; }
private:
	void *Thread();

	int m_device;
	int m_width, m_height;
	uint8_t *m_pCurFrame;
	uint8_t *m_pFullFrame;
	uint8_t *m_pMsgFrame;
	size_t m_frameSize;
	MIPRawYUV420PVideoMessage *m_pVideoMsg;
	JMutex m_frameMutex;
	JMutex m_stopMutex;
	MIPSignalWaiter m_sigWait;
	bool m_gotMsg, m_stopLoop;
	bool m_gotFrame;
	MIPTime m_captureTime;
	uint64_t m_sourceID;
};

#endif // MIPCONFIG_SUPPORT_VIDEO4LINUX

#endif // MIPV4LINPUT_H

