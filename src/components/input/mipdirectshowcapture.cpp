/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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

#ifdef MIPCONFIG_SUPPORT_DIRECTSHOW

#include "mipdirectshowcapture.h"
#include "mipsystemmessage.h"
#include "miprawvideomessage.h"
#include <iostream>

#define MIPDIRECTSHOWCAPTURE_ERRSTR_ALREADYOPEN								"Already a DirectShow capture device open"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATECAPTUREBUILDER				"Can't create a capture graph builder"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEMANAGER						"Can't create filter graph manager"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEDEVICEENUMERATOR				"Can't create device enumerator"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCAPTUREENUMERATOR					"Can't create video capture device enumerator"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTFINDCAPTUREDEVICE					"Can't find a useable capture device"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATENULLRENDERER					"Can't create null renderer"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEGRABFILTER					"Can't create sample grabber filter"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEGRABIFACE						"Unable to obtain sample grabber intereface"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSELECTYUV420P						"Unable to select YUV420P (IYUV) encoding"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSETCALLBACK							"Can't set grab callback"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTADDCAPTUREFILTER					"Can't add capture filter to filter graph"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTADDNULLRENDERER						"Can't add null renderer to filter graph"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTADDSAMPLEGRABBER					"Can't add sample grabber filer to filter graph"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTBUILDGRAPH							"Can't build render graph"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETFRAMESIZE						"Can't get frame size"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETCONTROLINTERFACE					"Can't get media control interface"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSTARTGRAPH							"Can't start filter graph"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTINITSIGWAIT							"Can't initialize signal waiter"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_DEVICENOTOPENED							"No DirectShow capture device was opened"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_BADMESSAGE								"Bad message"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETDEVICECONFIG						"Can't get device configuration"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETDEVICECAPS						"Can't get device capabilities"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_INVALIDCAPS								"Can't handle returned device capabilities"
#define MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSETCAPS								"Unable to install desired format"

MIPDirectShowCapture::MIPDirectShowCapture() : MIPComponent("MIPDirectShowCapture")
{
	zeroAll();
	
	int status;

	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize DirectShow capture frame mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPDirectShowCapture::~MIPDirectShowCapture()
{
	close();
}

bool MIPDirectShowCapture::open(int width, int height, real_t frameRate)
{
	if (m_pGraph != 0)
	{
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_ALREADYOPEN);
		return false;
	}

	if (!initCaptureGraphBuilder())
		return false;

	if (!getCaptureDevice())
	{
		clearNonZero();
		return false;
	}

	HRESULT hr;

	hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter,(void **)(&m_pNullRenderer));
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATENULLRENDERER);
		return false;
	}
	
	hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter,(void **)(&m_pSampGrabFilter));
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEGRABFILTER);
		return false;
	}

	hr = m_pSampGrabFilter->QueryInterface(IID_ISampleGrabber, (void**)&m_pGrabber);
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEGRABIFACE);
		return false;
	}

	AM_MEDIA_TYPE mt;

	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
	mt.majortype = MEDIATYPE_Video;
	mt.subtype = MEDIASUBTYPE_IYUV;
	hr = m_pGrabber->SetMediaType(&mt);
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSELECTYUV420P);
		return false;
	}

	m_pGrabCallback = new GrabCallback(this);

	hr = m_pGrabber->SetCallback(m_pGrabCallback, 1);
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSETCALLBACK);
		return false;
	}

	hr = m_pGraph->AddFilter(m_pCaptDevice, L"Capture Filter");
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTADDCAPTUREFILTER);
		return false;
	}

	if (!setFormat(width, height, frameRate))
	{
		clearNonZero();
		return false;
	}

	hr = m_pGraph->AddFilter(m_pNullRenderer, L"Null Renderer");
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTADDNULLRENDERER);
		return false;
	}

	hr = m_pGraph->AddFilter(m_pSampGrabFilter, L"Sample Grabber");
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTADDSAMPLEGRABBER);
		return false;
	}

	hr = m_pBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pCaptDevice, m_pSampGrabFilter, m_pNullRenderer);
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTBUILDGRAPH);
		return false;
	}

	hr = m_pGrabber->GetConnectedMediaType(&mt);
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETFRAMESIZE);
		return false;
	}

	VIDEOINFOHEADER * vih = (VIDEOINFOHEADER*) mt.pbFormat;
	m_width  = vih->bmiHeader.biWidth;
	m_height = vih->bmiHeader.biHeight;
	CoTaskMemFree(mt.pbFormat);

	hr = m_pGraph->QueryInterface(IID_IMediaControl, (void **)&m_pControl);
	if (FAILED(hr))
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETCONTROLINTERFACE);
		return false;
	}

	m_frameSize = (size_t)((m_width*m_height*3)/2);
	m_pFullFrame = new uint8_t [m_frameSize];
	m_pMsgFrame = new uint8_t [m_frameSize];
	memset(m_pMsgFrame, 0, m_frameSize);
	memset(m_pFullFrame, 0, m_frameSize);
	m_pVideoMsg = new MIPRawYUV420PVideoMessage(m_width, m_height, m_pMsgFrame, false);

	if (!m_sigWait.init())
	{
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTINITSIGWAIT);
		return false;
	}
	
	m_captureTime = MIPTime::getCurrentTime();
	m_gotMsg = false;
	m_gotFrame = false;

	hr = m_pControl->Run();
	if (FAILED(hr))
	{
		m_sigWait.destroy();
		clearNonZero();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSTARTGRAPH);
		return false;
	}

	m_sourceID = 0;

	return true;
}

bool MIPDirectShowCapture::close()
{
	if (m_pGraph == 0)
	{
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_DEVICENOTOPENED);
		return false;
	}

	m_pControl->Stop();
	clearNonZero();
	m_sigWait.destroy();

	return true;
}

bool MIPDirectShowCapture::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pGraph == 0)
	{
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_DEVICENOTOPENED);
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
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPDirectShowCapture::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pGraph == 0)
	{
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_DEVICENOTOPENED);
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

void MIPDirectShowCapture::clearNonZero()
{
	if (m_pControl)
		m_pControl->Release();
	if (m_pGrabber)
		m_pGrabber->Release();
	if (m_pSampGrabFilter)
		m_pSampGrabFilter->Release();
	if (m_pNullRenderer)
		m_pNullRenderer->Release();
	if (m_pCaptDevice)
		m_pCaptDevice->Release();
	if (m_pBuilder)
		m_pBuilder->Release();
	if (m_pGraph)
		m_pGraph->Release();
	if (m_pGrabCallback)
		delete m_pGrabCallback;
	if (m_pVideoMsg)
		delete m_pVideoMsg;
	if (m_pMsgFrame)
		delete [] m_pMsgFrame;
	if (m_pFullFrame)
		delete [] m_pFullFrame;
	zeroAll();
}

void MIPDirectShowCapture::zeroAll()
{
	m_pGraph = 0;
	m_pBuilder = 0;
	m_pControl = 0;
	m_pCaptDevice = 0;
	m_pNullRenderer = 0;
	m_pSampGrabFilter = 0;
	m_pGrabber = 0;
	m_pGrabCallback = 0;
	m_pFullFrame = 0;
	m_pMsgFrame = 0;
	m_pVideoMsg = 0;
}

bool MIPDirectShowCapture::initCaptureGraphBuilder()
{
    IGraphBuilder *pGraph = NULL;
    ICaptureGraphBuilder2 *pBuild = NULL;

    HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&pBuild);
    if (FAILED(hr))
    {
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATECAPTUREBUILDER);
		return false;
	}

    hr = CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&pGraph);
	if (FAILED(hr))
	{
		pBuild->Release();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEMANAGER);
		return false;
	}
	
	pBuild->SetFiltergraph(pGraph);

	m_pBuilder = pBuild;
	m_pGraph = pGraph;

	return true;
}

bool MIPDirectShowCapture::getCaptureDevice()
{
	ICreateDevEnum *pDevEnum = 0;
	IEnumMoniker *pEnum = 0;

	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)(&pDevEnum));
	if (FAILED(hr))
	{
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCREATEDEVICEENUMERATOR);
		return false;
	}

	hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
	if (FAILED(hr))
	{
		pDevEnum->Release();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTCAPTUREENUMERATOR);
		return false;
	}

	if (pEnum == 0)
	{
		pDevEnum->Release();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTFINDCAPTUREDEVICE);
		return false;
	}

	IMoniker *pMoniker = NULL;
	while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
	{
		hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&m_pCaptDevice);
		pMoniker->Release();
		if (SUCCEEDED(hr))
		{
			pEnum->Release();
			pDevEnum->Release();
			return true;
		}
		else
			m_pCaptDevice = 0;
	}
	pEnum->Release();
	pDevEnum->Release();
	setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTFINDCAPTUREDEVICE);
	return false;
}

bool MIPDirectShowCapture::setFormat(int w, int h, real_t rate)
{
	HRESULT hr;

	IAMStreamConfig *pConfig = 0;

	hr = m_pBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, 0, m_pCaptDevice, IID_IAMStreamConfig, (void**)&pConfig);
	if (FAILED(hr))
	{
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETDEVICECONFIG);
		return false;
	}

	int count = 0;
	int s = 0;
	
	hr = pConfig->GetNumberOfCapabilities(&count, &s);
	if (FAILED(hr))
	{
		pConfig->Release();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTGETDEVICECAPS);
		return false;
	}

	if (s != sizeof(VIDEO_STREAM_CONFIG_CAPS))
	{
		pConfig->Release();
		setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_INVALIDCAPS);
		return false;
	}

	for (int i = 0; i < count; i++)
	{
        VIDEO_STREAM_CONFIG_CAPS caps;
        AM_MEDIA_TYPE *pMediaType;

        hr = pConfig->GetStreamCaps(i, &pMediaType, (BYTE*)&caps);
        if (SUCCEEDED(hr))
        {
			if ((pMediaType->majortype == MEDIATYPE_Video) &&
				(pMediaType->subtype == MEDIASUBTYPE_IYUV) &&
				(pMediaType->formattype == FORMAT_VideoInfo) &&
				(pMediaType->cbFormat >= sizeof (VIDEOINFOHEADER)) &&
				(pMediaType->pbFormat != 0))
			{
				VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)pMediaType->pbFormat;
				
				pVih->bmiHeader.biWidth = w;
				pVih->bmiHeader.biHeight = h;
				pVih->bmiHeader.biSizeImage = DIBSIZE(pVih->bmiHeader);
				pVih->AvgTimePerFrame = (REFERENCE_TIME)(10000000.0/rate);

				hr = pConfig->SetFormat(pMediaType);
				if (SUCCEEDED(hr))
				{
					CoTaskMemFree(pMediaType->pbFormat);
					pConfig->Release();
					return true;
				}
			}

			if (pMediaType->pbFormat != 0)
				CoTaskMemFree(pMediaType->pbFormat);
		}
	}

	pConfig->Release();
	setErrorString(MIPDIRECTSHOWCAPTURE_ERRSTR_CANTSETCAPS);
	return false;
}

#endif // MIPCONFIG_SUPPORT_DIRECTSHOW
