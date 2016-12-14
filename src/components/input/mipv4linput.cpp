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

#ifdef MIPCONFIG_SUPPORT_VIDEO4LINUX

#include "mipv4linput.h"
#include "miptime.h"
#include "miprawvideomessage.h"
#include "mipsystemmessage.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/types.h>
#include <linux/videodev.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdlib>

#include "mipdebug.h"
#include <iostream>

#define MIPV4LINPUT_ERRSTR_ALREADYINIT				"A device is already opened"
#define MIPV4LINPUT_ERRSTR_CANTOPEN				"Can't open the device"
#define MIPV4LINPUT_ERRSTR_NOTINIT				"No device was opened"
#define MIPV4LINPUT_ERRSTR_CANTGETCAPABILITIES			"Can't get device capabilities"
#define MIPV4LINPUT_ERRSTR_CANTCAPTURE				"Device can't capture images"
#define MIPV4LINPUT_ERRSTR_CANTGETIMGPROP			"Can't get image properties"
#define MIPV4LINPUT_ERRSTR_CANTUNDERSTANDPALETTE		"Can't understand palette type"
#define MIPV4LINPUT_ERRSTR_BADMESSAGE				"Bad message"
#define MIPV4LINPUT_ERRSTR_CANTGETWINDOWINFO			"Can't get window info"
#define MIPV4LINPUT_ERRSTR_CANTSTARTTHREAD			"Can't start thread"
#define MIPV4LINPUT_ERRSTR_CANTINITSIGWAIT			"Can't initialize signal waiter"

MIPV4LInput::MIPV4LInput() : MIPComponent("MIPV4LInput")
{
	m_device = -1;

	int status;

	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize V4L frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize V4L stop mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPV4LInput::MIPV4LInput(const std::string &compName) : MIPComponent(compName)
{
	m_device = -1;

	// TODO: mutex init?
}

MIPV4LInput::~MIPV4LInput()
{
	close();
}

bool MIPV4LInput::open(const std::string &device, const MIPV4LInputParameters *pParams)
{
	if (m_device != -1)
	{
		setErrorString(MIPV4LINPUT_ERRSTR_ALREADYINIT);
		return false;
	}

	m_device = ::open(device.c_str(),O_RDONLY);
	if (m_device < 0)
	{
		m_device = -1;
		setErrorString(MIPV4LINPUT_ERRSTR_CANTOPEN);
		return false;
	}

	struct video_capability cap;
	
	if (ioctl(m_device,VIDIOCGCAP,&cap) == -1)
	{
		::close(m_device);
		m_device = -1;
		setErrorString(MIPV4LINPUT_ERRSTR_CANTGETCAPABILITIES);
		return false;
	}
	if ((cap.type&VID_TYPE_CAPTURE) != VID_TYPE_CAPTURE)
	{
		::close(m_device);
		m_device = -1;
		setErrorString(MIPV4LINPUT_ERRSTR_CANTCAPTURE);
		return false;
	}
	
	if (!subInit(m_device,pParams))
	{
		::close(m_device);
		m_device = -1;
		return false;
	}

	struct video_window win;

	if (ioctl(m_device,VIDIOCGWIN,&win) == -1)
	{
		::close(m_device);
		m_device = -1;
		setErrorString(MIPV4LINPUT_ERRSTR_CANTGETWINDOWINFO);
		return false;
	}
	
	m_width = win.width;
	m_height = win.height;
	
	int subtype = 0;
	int allocSize = m_width*m_height;
	struct video_picture pic;
	
	if (ioctl(m_device,VIDIOCGPICT,&pic) == -1)
	{
		::close(m_device);
		m_device = -1;
		setErrorString(MIPV4LINPUT_ERRSTR_CANTGETIMGPROP);
		return false;
	}
	
	if (pic.palette == VIDEO_PALETTE_YUV420P)
	{
		subtype = MIPRAWVIDEOMESSAGE_TYPE_YUV420P;

		int w2 = m_width;
		int h2 = m_height;
		
		if (w2&1) w2++;
		if (h2&1) h2++;
		
		allocSize = (w2*h2*3)/2;
	}
	else
	{
		::close(m_device);
		m_device = -1;
		setErrorString(MIPV4LINPUT_ERRSTR_CANTUNDERSTANDPALETTE);
		return false;
	}

	if (!m_sigWait.init())
	{
		::close(m_device);
		m_device = -1;
		setErrorString(MIPV4LINPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}

	m_frameSize = (size_t)allocSize;
	
	m_pCurFrame = new uint8_t [allocSize];
	m_pFullFrame = new uint8_t [allocSize];
	m_pMsgFrame = new uint8_t [allocSize];
	m_pVideoMsg = new MIPRawYUV420PVideoMessage(m_width, m_height, m_pMsgFrame, false);
	m_gotMsg = false;
	m_stopLoop = false;

	if (JThread::Start() < 0)
	{
		delete [] m_pMsgFrame;
		delete [] m_pFullFrame;
		delete [] m_pCurFrame;
		delete m_pVideoMsg;
		::close(m_device);
		m_device = -1;
		m_sigWait.destroy();
		setErrorString(MIPV4LINPUT_ERRSTR_CANTSTARTTHREAD);
		return false;
	}

	m_captureTime = MIPTime::getCurrentTime();
	m_gotFrame = true;
	m_sourceID = 0;
	
	return true;
}

bool MIPV4LInput::close()
{
	if (m_device == -1)
	{
		setErrorString(MIPV4LINPUT_ERRSTR_NOTINIT);
		return false;
	}

	m_stopMutex.Lock();
	m_stopLoop = true;
	m_stopMutex.Unlock();
	
	MIPTime startTime = MIPTime::getCurrentTime();

	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue() - startTime.getValue()) < 5.0)
		MIPTime::wait(MIPTime(0.020));

	if (JThread::IsRunning())
		JThread::Kill();

	delete [] m_pMsgFrame;
	delete [] m_pFullFrame;
	delete [] m_pCurFrame;
	delete m_pVideoMsg;

	m_sigWait.destroy();

	::close(m_device);
	m_device = -1;
	
	return true;
}

bool MIPV4LInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_device == -1)
	{
		setErrorString(MIPV4LINPUT_ERRSTR_NOTINIT);
		return false;
	}
	
	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_ISTIME)
	{
		m_frameMutex.Lock();
		memcpy(m_pMsgFrame, m_pFullFrame, m_frameSize);
		m_pVideoMsg->setTime(m_captureTime);
		m_pVideoMsg->setSourceID(m_sourceID);
		m_frameMutex.Unlock();

		m_gotMsg = false;
	}
	else if (pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME)
	{
		bool waitForFrame = false;
		
		m_frameMutex.Lock();
		if (!m_gotFrame)
		{
			memcpy(m_pMsgFrame, m_pFullFrame, m_frameSize);
			m_pVideoMsg->setTime(m_captureTime);
			m_gotFrame = true;
		}
		else
			waitForFrame = true;
		m_frameMutex.Unlock();

		if (waitForFrame)
		{
			m_sigWait.clearSignalBuffers();
			m_sigWait.waitForSignal();

			m_frameMutex.Lock();
			memcpy(m_pMsgFrame, m_pFullFrame, m_frameSize);
			m_pVideoMsg->setTime(m_captureTime);
			m_pVideoMsg->setSourceID(m_sourceID);
			m_gotFrame = true;
			m_frameMutex.Unlock();
		}

		m_gotMsg = false;	
	}
	else
	{
		setErrorString(MIPV4LINPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPV4LInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_device == -1)
	{
		setErrorString(MIPV4LINPUT_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_gotMsg)
	{
		m_gotMsg = false;
		*pMsg = 0;
	}
	else
	{
		m_gotMsg = true;
		*pMsg = m_pVideoMsg;
	}
	return true;
}

void *MIPV4LInput::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPV4LInput::Thread started" << std::endl;
#endif // MIPDEBUG

	JThread::ThreadStarted();

	bool done;
	uint8_t *pTmp;

	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();
	
	MIPTime prevTime = MIPTime::getCurrentTime();
	
	while (!done)
	{
		read(m_device, m_pCurFrame, m_frameSize);

		MIPTime curTime = MIPTime::getCurrentTime();
		MIPTime cycleTime = curTime;
		cycleTime -= prevTime;
		prevTime = curTime;
		
		m_frameMutex.Lock();
		pTmp = m_pCurFrame;
		m_pCurFrame = m_pFullFrame;
		m_pFullFrame = pTmp;
		m_gotFrame = false;
		m_captureTime = MIPTime::getCurrentTime();
		m_captureTime -= cycleTime;
		m_frameMutex.Unlock();
		
		m_sigWait.signal();

		m_stopMutex.Lock();
		done = m_stopLoop;
		m_stopMutex.Unlock();

	}

#ifdef MIPDEBUG
	std::cout << "MIPV4LInput::Thread stopped" << std::endl;
#endif // MIPDEBUG

	return 0;
}

#endif // MIPCONFIG_SUPPORT_VIDEO4LINUX

