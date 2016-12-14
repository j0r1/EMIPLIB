/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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
 * \file mipdirectshowcapture.h
 */

#ifndef MIPDIRECTSHOWCAPTURE_H

#define MIPDIRECTSHOWCAPTURE_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_DIRECTSHOW

#include "mipcomponent.h"
#include "mipsignalwaiter.h"
#include "miptime.h"
#include <dshow.h>
#include <Qedit.h>

class MIPRawYUV420PVideoMessage;

/** A DirectShow input component.
 *  This component is a DirectShow input component, e.g. for a webcam. It accepts
 *  both MIPSYSTEMMESSAGE_TYPE_WAITTIME and MIPSYSTEMMESSAGE_TYPE_ISTIME messages
 *  and created YUV420P encoded raw video messages.
 */
class MIPDirectShowCapture : public MIPComponent
{
public:
	MIPDirectShowCapture();
	~MIPDirectShowCapture();

	/** Initializes the component.
	 *  This function initializes the component.
	 *  \param width Width of the video frames.
	 *  \param height Height of the video frames.
	 *  \param frameRate Framerate.
	 *  \param devideNumber 0 opens the first device which is encountered, 1 the second, etc.
	 */
	bool open(int width, int height, real_t frameRate, int deviceNumber = 0);

	/** Closes the DirectShow input device. */
	bool close();

	/** Returns the width of captured video frames. */
	int getWidth() const											{ if (m_pGraph == 0) return -1; return m_width; }

	/** Returns the height of captured video frames. */
	int getHeight()	const											{ if (m_pGraph == 0) return -1; return m_height; }

	/** Sets the source ID to be stored in generated messages. */
	void setSourceID(uint64_t srcID)									{ m_sourceID = srcID; }

	/** Returns the source ID which will be stored in generated messages. */
	uint64_t getSourceID() const										{ return m_sourceID; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void zeroAll();
	void clearNonZero();
	bool initCaptureGraphBuilder();
	bool getCaptureDevice(int deviceNumber);
	bool setFormat(int w, int h, real_t rate);

	class GrabCallback : public ISampleGrabberCB
	{
	public:
		GrabCallback(MIPDirectShowCapture *pDSCapt)										{ m_pDSCapt = pDSCapt; }
		STDMETHODIMP_(ULONG) AddRef()													{ return 2; }
		STDMETHODIMP_(ULONG) Release()													{ return 1; }

		STDMETHODIMP QueryInterface(REFIID riid, void ** ppv)
		{	
			if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown ) 
			{
				*ppv = (void *) static_cast<ISampleGrabberCB*> ( this );
				return NOERROR;
			}    

			return E_NOINTERFACE;
		}

		STDMETHODIMP SampleCB( double SampleTime, IMediaSample * pSample )
		{
			return 0;
		}

		STDMETHODIMP BufferCB( double dblSampleTime, BYTE * pBuffer, long lBufferSize )
		{
			size_t minsize = (size_t)lBufferSize;

			if (minsize > m_pDSCapt->m_frameSize)
				minsize = m_pDSCapt->m_frameSize;

			m_pDSCapt->m_frameMutex.Lock();
			memcpy(m_pDSCapt->m_pFullFrame, pBuffer, minsize);
			m_pDSCapt->m_gotFrame = false;
			m_pDSCapt->m_captureTime = MIPTime::getCurrentTime();
			m_pDSCapt->m_frameMutex.Unlock();
		
			m_pDSCapt->m_sigWait.signal();
			return 0;
		}
	private:
		MIPDirectShowCapture *m_pDSCapt;
	};

	IGraphBuilder *m_pGraph;
	ICaptureGraphBuilder2 *m_pBuilder;
	IMediaControl *m_pControl;
	IBaseFilter *m_pCaptDevice;
	IBaseFilter *m_pNullRenderer;
	IBaseFilter *m_pSampGrabFilter;
	ISampleGrabber *m_pGrabber;
	GrabCallback *m_pGrabCallback;

	int m_width;
	int m_height;
	uint8_t *m_pFullFrame;
	uint8_t *m_pMsgFrame;
	size_t m_frameSize;
	MIPRawYUV420PVideoMessage *m_pVideoMsg;
	JMutex m_frameMutex;
	MIPSignalWaiter m_sigWait;
	bool m_gotMsg;
	bool m_gotFrame;
	MIPTime m_captureTime;
	uint64_t m_sourceID;
};

#endif // MIPCONFIG_SUPPORT_DIRECTSHOW

#endif // MIPDIRECTSHOWCAPTURE_H
