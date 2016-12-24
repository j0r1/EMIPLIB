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

#ifdef MIPCONFIG_SUPPORT_VIDEO4LINUX

#include "mipv4l2input.h"
#include "miptime.h"
#include "miprawvideomessage.h"
#include "mipencodedvideomessage.h"
#include "mipsystemmessage.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <iostream>

#include "mipdebug.h"

#define MIPV4L2INPUT_ERRSTR_ALREADYINIT				"A device is already opened"
#define MIPV4L2INPUT_ERRSTR_CANTOPEN				"Can't open the device"
#define MIPV4L2INPUT_ERRSTR_NOTINIT				"No device was opened"
#define MIPV4L2INPUT_ERRSTR_CANTGETCAPABILITIES			"Can't get device capabilities"
#define MIPV4L2INPUT_ERRSTR_CANTCAPTURE				"Device can't capture images"
#define MIPV4L2INPUT_ERRSTR_CANTUNDERSTANDPALETTE		"Can't understand palette type"
#define MIPV4L2INPUT_ERRSTR_BADMESSAGE				"Bad message"
#define MIPV4L2INPUT_ERRSTR_CANTGETFORMATINFO			"Can't get format info"
#define MIPV4L2INPUT_ERRSTR_CANTSETFORMATINFO			"Can't set format info"
#define MIPV4L2INPUT_ERRSTR_CANTSTARTTHREAD			"Can't start thread"
#define MIPV4L2INPUT_ERRSTR_CANTINITSIGWAIT			"Can't initialize signal waiter"
#define MIPV4L2INPUT_ERRSTR_UNEXPECTEDIMAGESIZE			"Unexpected image size"
#define MIPV4L2INPUT_ERRSTR_CANTGETSTREAMPARAMETERS		"Unable to get stream parameters"
#define MIPV4L2INPUT_ERRSTR_CANTGETSTANDARDINFO			"Unable to get standard info"
#define MIPV4L2INPUT_ERRSTR_CANTUNDERSTANDFORMAT		"Can't understand picture format"
#define MIPV4L2INPUT_ERRSTR_SHOULDNTHAPPEN			"This shouldn't happen (internal error)"
#define MIPV4L2INPUT_ERRSTR_CANTREADWRITEORUSERPTRORMMAP	"Device doesn't support read/write interface or user pointer interface or memory mapping"
#define MIPV4L2INPUT_ERRSTR_NOTENOUGHMMAPBUFFERSANDNOREADWRITE	"Not enough mmap buffers available and no read/write support"

#define MIPV4L2INPUT_NUMUSERPTRBUFFERS				4
#define MIPV4L2INPUT_NUMMMAPBUFFERS				4

MIPV4L2Input::MIPV4L2Input() : MIPComponent("MIPV4L2Input")
{
	m_device = -1;

	int status;

	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize V4L2 frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	if ((status = m_stopMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize V4L2 stop mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}

	m_pMsgFrame = 0;
	m_pFullFrame = 0;
	m_pCurFrame = 0;
	m_pVideoMsg = 0;
}

MIPV4L2Input::~MIPV4L2Input()
{
	close();
}

bool MIPV4L2Input::open(int reqWidth, int reqHeight, const std::string &device)
{
	if (m_device != -1)
	{
		setErrorString(MIPV4L2INPUT_ERRSTR_ALREADYINIT);
		return false;
	}

	m_device = ::open(device.c_str(),O_RDONLY);
	if (m_device < 0)
	{
		m_device = -1;
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTOPEN);
		return false;
	}

	struct v4l2_capability capabilities;

	memset(&capabilities, 0, sizeof(struct v4l2_capability));

	if (ioctl(m_device, VIDIOC_QUERYCAP, &capabilities) == -1)
	{
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTGETCAPABILITIES);
		return false;
	}

	if ((capabilities.capabilities&V4L2_CAP_VIDEO_CAPTURE) != V4L2_CAP_VIDEO_CAPTURE)
	{
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTCAPTURE);
		return false;
	}

	if (!((capabilities.capabilities&V4L2_CAP_READWRITE) == V4L2_CAP_READWRITE || 
              (capabilities.capabilities&V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING ) )
	{
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTREADWRITEORUSERPTRORMMAP);
		return false;
	}

	if ((capabilities.capabilities&V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING)	
		m_streaming = true;
	else
		m_streaming = false;

	struct v4l2_format format;

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(m_device, VIDIOC_G_FMT, &format) == -1)
	{
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTGETFORMATINFO);
		return false;
	}

	format.fmt.pix.field = V4L2_FIELD_NONE;
	format.fmt.pix.width = reqWidth;
	format.fmt.pix.height = reqHeight;

	// First, try to set picture size
	ioctl(m_device, VIDIOC_S_FMT, &format);

	// Try some formats
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
	ioctl(m_device, VIDIOC_S_FMT, &format);

	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	ioctl(m_device, VIDIOC_S_FMT, &format);

	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	ioctl(m_device, VIDIOC_S_FMT, &format);

	if (ioctl(m_device, VIDIOC_G_FMT, &format) == -1)
	{
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTGETFORMATINFO);
		return false;
	}
	
	m_width = format.fmt.pix.width;
	m_height = format.fmt.pix.height;

	switch (format.fmt.pix.pixelformat)
	{
	case V4L2_PIX_FMT_JPEG:
		m_compressed = true;
		break;
	case V4L2_PIX_FMT_YUYV:
		m_compressed = false;
		break;
	case V4L2_PIX_FMT_YUV420:
		m_compressed = false;
		break;
	default:
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTUNDERSTANDFORMAT);
		return false;
	}

	if (!m_sigWait.init())
	{
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTINITSIGWAIT);
		return false;
	}

	m_frameSize = (size_t)format.fmt.pix.sizeimage;
	
	m_pCurFrame = 0;
	m_pFullFrame = new uint8_t [m_frameSize];
	m_pMsgFrame = new uint8_t [m_frameSize];

	switch (format.fmt.pix.pixelformat)
	{
	case V4L2_PIX_FMT_JPEG:
		m_pVideoMsg = new MIPEncodedVideoMessage(MIPENCODEDVIDEOMESSAGE_TYPE_JPEG, m_width, m_height, m_pMsgFrame, m_frameSize, false);
		m_subType = MIPENCODEDVIDEOMESSAGE_TYPE_JPEG;
		break;
	case V4L2_PIX_FMT_YUYV:
		m_pVideoMsg = new MIPRawYUYVVideoMessage(m_width, m_height, m_pMsgFrame, false);
		m_subType = MIPRAWVIDEOMESSAGE_TYPE_YUYV;
		break;
	case V4L2_PIX_FMT_YUV420:
		m_pVideoMsg = new MIPRawYUV420PVideoMessage(m_width, m_height, m_pMsgFrame, false);
		m_subType = MIPRAWVIDEOMESSAGE_TYPE_YUV420P;
		break;
	default:
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_SHOULDNTHAPPEN);
		return false;
	}

	if (m_streaming)
	{
		// first try user pointer method

		m_userPtr = true;

		struct v4l2_requestbuffers requestBuffer;

		memset(&requestBuffer, 0, sizeof(struct v4l2_requestbuffers));
		requestBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		requestBuffer.memory = V4L2_MEMORY_USERPTR;
		requestBuffer.count = MIPV4L2INPUT_NUMUSERPTRBUFFERS;

		if (ioctl(m_device, VIDIOC_REQBUFS, &requestBuffer) == -1) 
		{
			// try mmap method
			
			memset(&requestBuffer, 0, sizeof(struct v4l2_requestbuffers));
			requestBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			requestBuffer.memory = V4L2_MEMORY_MMAP;
			requestBuffer.count = MIPV4L2INPUT_NUMMMAPBUFFERS;
			
			if (ioctl(m_device, VIDIOC_REQBUFS, &requestBuffer) == -1) 
			{
				if ((capabilities.capabilities&V4L2_CAP_READWRITE) != V4L2_CAP_READWRITE)
				{
					cleanUp();
					setErrorString(MIPV4L2INPUT_ERRSTR_CANTREADWRITEORUSERPTRORMMAP);
					return false;
				}
				m_streaming = false; // resort to read/write method
			}
			else // ioctl was successful
			{
				// check available buffers
				
				if (requestBuffer.count != MIPV4L2INPUT_NUMMMAPBUFFERS)
				{
					if ((capabilities.capabilities&V4L2_CAP_READWRITE) != V4L2_CAP_READWRITE)
					{
						cleanUp();
						setErrorString(MIPV4L2INPUT_ERRSTR_NOTENOUGHMMAPBUFFERSANDNOREADWRITE);
						return false;
					}
					m_streaming = false; // resort to read/write method
				}
				else // requested buffers available
				{
					m_userPtr = false; 

					// NOTE: We do the rest of the setup in the thread
				}
			}
		}

		// NOTE: We do the rest of the setup in the thread
	}
	else
		m_pCurFrame = new uint8_t[m_frameSize];

	m_gotMsg = false;
	m_stopLoop = false;

	if (JThread::Start() < 0)
	{
		cleanUp();
		setErrorString(MIPV4L2INPUT_ERRSTR_CANTSTARTTHREAD);
		return false;
	}

	m_captureTime = MIPTime::getCurrentTime();
	m_gotFrame = true;
	m_sourceID = 0;

	// TODO: timing?
	
	return true;
}

bool MIPV4L2Input::close()
{
	if (m_device == -1)
	{
		setErrorString(MIPV4L2INPUT_ERRSTR_NOTINIT);
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

	cleanUp();
	
	return true;
}

bool MIPV4L2Input::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_device == -1)
	{
		setErrorString(MIPV4L2INPUT_ERRSTR_NOTINIT);
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
		setErrorString(MIPV4L2INPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPV4L2Input::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_device == -1)
	{
		setErrorString(MIPV4L2INPUT_ERRSTR_NOTINIT);
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

void *MIPV4L2Input::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPV4L2Input::Thread started" << std::endl;
#endif // MIPDEBUG

	JThread::ThreadStarted();

	bool done;
	uint8_t *pTmp;

	m_stopMutex.Lock();
	done = m_stopLoop;
	m_stopMutex.Unlock();
	
	MIPTime prevTime = MIPTime::getCurrentTime();
	
	if (!m_streaming) // read/write method
	{
		while (!done)
		{
			int status;

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
	}
	else // streaming method
	{
		if (m_userPtr)
		{
			struct v4l2_buffer buffer;
			unsigned int pageSize;
			void *pBuffers[MIPV4L2INPUT_NUMUSERPTRBUFFERS];

			pageSize = getpagesize();

			for (int i = 0 ; i < MIPV4L2INPUT_NUMUSERPTRBUFFERS ; i++)
			{
				pBuffers[i] = memalign(pageSize, m_frameSize);
			
				// TODO: check out of memory

				memset(&buffer, 0, sizeof(struct v4l2_buffer));
				buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buffer.memory = V4L2_MEMORY_USERPTR;
				buffer.m.userptr = (unsigned long)pBuffers[i];
				buffer.length = m_frameSize;
				buffer.index = i;

				ioctl(m_device, VIDIOC_QBUF, &buffer);
			}

			int argType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			ioctl(m_device, VIDIOC_STREAMON, &argType);

			while (!done)
			{
				memset(&buffer, 0, sizeof(struct v4l2_buffer));
				buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buffer.memory = V4L2_MEMORY_USERPTR;
		
				// Wait to dequeue a buffer
				ioctl(m_device, VIDIOC_DQBUF, &buffer);

				MIPTime curTime = MIPTime::getCurrentTime();
				MIPTime cycleTime = curTime;
				cycleTime -= prevTime;
				prevTime = curTime;
				
				m_frameMutex.Lock();
				memcpy(m_pFullFrame, (void *)buffer.m.userptr, m_frameSize);
				m_gotFrame = false;
				m_captureTime = MIPTime::getCurrentTime();
				m_captureTime -= cycleTime;
				m_frameMutex.Unlock();
				
				// Enqueue buffer again
				ioctl(m_device, VIDIOC_QBUF, &buffer);

				m_sigWait.signal();

				m_stopMutex.Lock();
				done = m_stopLoop;
				m_stopMutex.Unlock();
			}

			ioctl(m_device, VIDIOC_STREAMOFF, &argType);

			for (int i = 0 ; i < MIPV4L2INPUT_NUMUSERPTRBUFFERS ; i++)
				free(pBuffers[i]);
		}
		else // mmap method
		{
			struct v4l2_buffer buffer;
			void *pMemory[MIPV4L2INPUT_NUMMMAPBUFFERS];
			size_t lengths[MIPV4L2INPUT_NUMMMAPBUFFERS];

			int status;

			for (int i = 0 ; i < MIPV4L2INPUT_NUMMMAPBUFFERS ; i++)
			{
				memset(&buffer, 0, sizeof(struct v4l2_buffer));
				buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buffer.memory = V4L2_MEMORY_MMAP;
				buffer.index = i;

				ioctl(m_device, VIDIOC_QUERYBUF, &buffer);

				pMemory[i] = mmap(0, buffer.length, PROT_READ, MAP_SHARED, m_device, buffer.m.offset);
				lengths[i] = buffer.length;
			}

			for (int i = 0 ; i < MIPV4L2INPUT_NUMMMAPBUFFERS ; i++)
			{
				memset(&buffer, 0, sizeof(struct v4l2_buffer));
				buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buffer.memory = V4L2_MEMORY_MMAP;
				buffer.index = i;

				ioctl(m_device, VIDIOC_QBUF, &buffer);
			}

			int argType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			ioctl(m_device, VIDIOC_STREAMON, &argType);

			while (!done)
			{
				memset(&buffer, 0, sizeof(struct v4l2_buffer));
				buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buffer.memory = V4L2_MEMORY_MMAP;
		
				// Wait to dequeue a buffer
				ioctl(m_device, VIDIOC_DQBUF, &buffer);

				MIPTime curTime = MIPTime::getCurrentTime();
				MIPTime cycleTime = curTime;
				cycleTime -= prevTime;
				prevTime = curTime;
				
				m_frameMutex.Lock();
				memcpy(m_pFullFrame, pMemory[buffer.index], m_frameSize);
				m_gotFrame = false;
				m_captureTime = MIPTime::getCurrentTime();
				m_captureTime -= cycleTime;
				m_frameMutex.Unlock();
				
				// Enqueue buffer again
				ioctl(m_device, VIDIOC_QBUF, &buffer);

				m_sigWait.signal();

				m_stopMutex.Lock();
				done = m_stopLoop;
				m_stopMutex.Unlock();
			}

			ioctl(m_device, VIDIOC_STREAMOFF, &argType);

			for (int i = 0 ; i < MIPV4L2INPUT_NUMMMAPBUFFERS ; i++)
				munmap(pMemory[i], lengths[i]);
		}
	}

#ifdef MIPDEBUG
	std::cout << "MIPV4LInput::Thread stopped" << std::endl;
#endif // MIPDEBUG

	return 0;
}

void MIPV4L2Input::cleanUp()
{
	if (m_pMsgFrame)
	{
		delete [] m_pMsgFrame;
		m_pMsgFrame = 0;
	}
	if (m_pFullFrame)
	{
		delete [] m_pFullFrame;
		m_pFullFrame = 0;
	}
	if (m_pCurFrame)
	{
		delete [] m_pCurFrame;
		m_pCurFrame = 0;
	}
	if (m_pVideoMsg)
	{
		delete m_pVideoMsg;
		m_pVideoMsg = 0;
	}
	
	if (m_device != -1)
	{
		::close(m_device);
		m_device = -1;
	}
	if (m_sigWait.isInit())
		m_sigWait.destroy();
}

#endif // MIPCONFIG_SUPPORT_VIDEO4LINUX

