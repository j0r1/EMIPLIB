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

#ifdef MIPCONFIG_SUPPORT_OPENSLESANDROID

#include "mipopenslesandroidinput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include "mipstreambuffer.h"
#include "mipsignalwaiter.h"
#include <vector>
#include <sstream>

using namespace std;

// TODO: what's a good value for smooth recording? Does this introduce delay?
const int numRecordBuffers = 10;

#define MIPOPENSLESANDROIDINPUT_ERRSTR_ALREADYINIT "The OpenSL ES audio output component is already initialized"
#define MIPOPENSLESANDROIDINPUT_ERRSTR_NOTINIT "The OpenSL ES audio output component is not initialized"
#define MIPOPENSLESANDROIDINPUT_ERRSTR_NOPULL "Pull is not supported"

MIPOpenSLESAndroidInput::MIPOpenSLESAndroidInput() : MIPComponent("MIPOpenSLESAndroidInput")
{
	m_init = false;
}

MIPOpenSLESAndroidInput::~MIPOpenSLESAndroidInput()
{
	close();
}

class InterfacesToDestroyOnError
{
public:
	InterfacesToDestroyOnError() { }
	~InterfacesToDestroyOnError() 
	{
		// Destroy in reverse order
		for (int i = m_ifaces.size()-1 ; i >= 0 ; i--)
		{
			auto iface = m_ifaces[i];
			(*iface)->Destroy(iface);
		}
	}
	void add(SLObjectItf i) { m_ifaces.push_back(i); }
private:
	vector<SLObjectItf> m_ifaces;
};

bool MIPOpenSLESAndroidInput::open(int sampRate, int channels, MIPTime interval)
{
	if (m_init)
	{
		setErrorString(MIPOPENSLESANDROIDINPUT_ERRSTR_ALREADYINIT);
		return false;
	}

	if (channels < 1 || channels > 2)
	{
		setErrorString("Unsupported number of channels");
		return false;
	}

	int numFrames = (int)((double)sampRate*interval.getValue() + 0.5);
	m_blockSize = numFrames * sizeof(int16_t) * channels;
	m_sampRate = sampRate;
	m_channels = channels;

	m_pSigWait = new MIPSignalWaiter();
	if (!m_pSigWait->init())
	{
		setErrorString("Unable to initialize signal waiter");
		delete m_pSigWait;
		return false;
	}
	m_pBuffer = new MIPStreamBuffer(m_blockSize, 10);
	m_pThread = new AudioThread(m_pBuffer, m_blockSize, sampRate, channels, interval, m_pSigWait);
	if (m_pThread->Start() < 0)
	{
		delete m_pThread;
		delete m_pBuffer;
		delete m_pSigWait;
		setErrorString("Can't start audio thread");
		return false;
	}

	// TODO: do something cleaner here instead of waiting
	while (m_pThread->IsRunning() && !m_pThread->successFullyStarted())
		MIPTime::wait(MIPTime(0.100));

	if (!m_pThread->IsRunning())
	{
		setErrorString("Unable to successfully start audio thread: " + m_pThread->getErrorString());
		delete m_pThread;
		delete m_pBuffer;
		delete m_pSigWait;
		return false;
	}

	m_frames.resize(numFrames*channels);
	m_pMsg = new MIPRaw16bitAudioMessage(sampRate, channels, numFrames, true, MIPRaw16bitAudioMessage::Native, &m_frames[0], false);
	m_gotMsg = false;

	m_init = true;

	return true;
}

bool MIPOpenSLESAndroidInput::close()
{
	if (!m_init)
	{
		setErrorString(MIPOPENSLESANDROIDINPUT_ERRSTR_NOTINIT);
		return false;
	}

	delete m_pThread;
	delete m_pBuffer;
	delete m_pSigWait;
	delete m_pMsg;

	m_init = false;
	return true;
}

bool MIPOpenSLESAndroidInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPOPENSLESANDROIDINPUT_ERRSTR_NOTINIT);
		return false;
	}

	if (pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME)
	{
		if (iteration == 1) // at the start, wait for the first new buffer
			m_pSigWait->clearSignalBuffers();

		m_pSigWait->waitForSignal();

		if (m_pBuffer->read(&m_frames[0], m_blockSize) != m_blockSize)
		{
			setErrorString("Incomplete read");
			return false;
		}

		m_gotMsg = false;
	}
	else
	{
		setErrorString("Bad message");
		return false;
	}
	return true;
}

bool MIPOpenSLESAndroidInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPOPENSLESANDROIDINPUT_ERRSTR_NOTINIT);
		return false;
	}

	if (m_gotMsg)
	{
		m_gotMsg = false;
		*pMsg = nullptr;
	}
	else
	{
		m_gotMsg = true;
		*pMsg = m_pMsg;
	}

	return true;
}

MIPOpenSLESAndroidInput::AudioThread::AudioThread(MIPStreamBuffer *pBuffer, int blockSize, int sampRate, int channels, 
                                                  MIPTime interval, MIPSignalWaiter *pSigWait)
{
	m_stopFlag = false;
	m_successFullyStarted = false;

	m_pSigWait = pSigWait;
	m_pBuffer = pBuffer;
	m_blockSize = blockSize;
	m_sampRate = sampRate;
	m_channels = channels;
	m_interval = interval;

	m_buffers.resize(numRecordBuffers);
	for (size_t i = 0 ; i < m_buffers.size() ; i++)
		m_buffers[i].resize(m_blockSize, 0);
}

MIPOpenSLESAndroidInput::AudioThread::~AudioThread()
{
	m_stopFlag = true;

	MIPTime startTime = MIPTime::getCurrentTime();
	while (IsRunning())
	{
		MIPTime::wait(MIPTime(0.100));
		
		MIPTime curTime = MIPTime::getCurrentTime();

		if (curTime.getValue() - startTime.getValue() > 10.0)
			break;
	}

	if (IsRunning())
		Kill();
}

void *MIPOpenSLESAndroidInput::AudioThread::Thread()
{
	JThread::ThreadStarted();

	// TODO: set high priority?

	InterfacesToDestroyOnError interfacesToDestroyOnError;

	SLObjectItf engineObj;
	auto retVal = slCreateEngine(&engineObj, 0, nullptr, 0, nullptr, nullptr);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't create OpenSL Engine object";
		return 0;
	}
	interfacesToDestroyOnError.add(engineObj);

	retVal = (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't realize engine object";
		return 0;
	}

	SLEngineItf engine;
	retVal = (*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engine);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't get engine interface";
		return 0;
	}

	SLDataLocator_IODevice locDev = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, 
	                                  SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr };
	SLDataSource source;
	source.pLocator = (void *)&locDev;

	SLDataLocator_AndroidSimpleBufferQueue queueLoc;
	queueLoc.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
	queueLoc.numBuffers = m_buffers.size();

	SLDataFormat_PCM format;
	format.formatType = SL_DATAFORMAT_PCM;
	format.numChannels = m_channels;
	format.samplesPerSec = m_sampRate * 1000; // From the spec: Note: This is set to milliHertz and not Hertz, as the field name would suggest
	format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	if (m_channels == 1)
		format.channelMask = SL_SPEAKER_FRONT_CENTER;
	else // channels == 2
		format.channelMask = SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT;

	// To use native endianness
#ifdef MIPCONFIG_BIGENDIAN 
	format.endianness = SL_BYTEORDER_BIGENDIAN;
#else
	format.endianness = SL_BYTEORDER_LITTLEENDIAN;
#endif

	SLDataSink sink;
	sink.pLocator = (void *)&queueLoc;
	sink.pFormat = (void * )&format;

	SLObjectItf recorder;
	SLInterfaceID interfaceIDs[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
	SLboolean requiredFlags[] = { SL_BOOLEAN_TRUE } ;
	retVal = (*engine)->CreateAudioRecorder(engine, &recorder, &source, &sink, 1, interfaceIDs, requiredFlags);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't create audio recorder interface";
		return 0;
	}
	interfacesToDestroyOnError.add(recorder);

	retVal = (*recorder)->Realize(recorder, SL_BOOLEAN_FALSE);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't realize recorder";
		return 0;
	}

	SLRecordItf recorderIface;
	retVal = (*recorder)->GetInterface(recorder, SL_IID_RECORD, (void *)&recorderIface);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't get recorder interface";
		return 0;
	}

	retVal = (*recorder)->GetInterface(recorder, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &m_recordBuffer);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't get recorder buffer interface";
		return 0;
	}

	retVal = (*m_recordBuffer)->RegisterCallback(m_recordBuffer, staticBufferQueueCallback, this);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't register callback";
		return 0;
	}

	for (size_t i = 0 ; i < m_buffers.size() ; i++)
	{
		retVal = (*m_recordBuffer)->Enqueue(m_recordBuffer, &(m_buffers[i][0]), m_buffers[i].size());
		if (retVal != SL_RESULT_SUCCESS)
		{
			m_errorString = "Unable to enqueue a buffer for recording";
			return 0;
		}
	}
	m_bufCount = 0;

	retVal = (*recorderIface)->SetRecordState(recorderIface, SL_RECORDSTATE_RECORDING);
	if (retVal != 0)
	{
		m_errorString = "Can't start recording";
		return 0;
	}

	m_successFullyStarted = true;

	MIPTime startTime = MIPTime::getCurrentTime();
	int64_t count = 0;
	while (!m_stopFlag)
	{
		// Just wait until we're finished, the callback
		// should do the rest
		MIPTime::wait(MIPTime(0.100));
	}

	(*recorderIface)->SetRecordState(recorderIface, SL_RECORDSTATE_STOPPED);

	// The destructor of interfacesToDestroyOnError will do the de-initialization
	return 0;
}

void MIPOpenSLESAndroidInput::AudioThread::bufferQueueCallback(SLAndroidSimpleBufferQueueItf caller)
{
	uint8_t *pData = &(m_buffers[m_bufCount][0]);
	m_bufCount++;
	if (m_bufCount >= m_buffers.size())
		m_bufCount = 0;

	m_pBuffer->write(pData, m_blockSize);
	m_pSigWait->signal();

	auto retVal = (*m_recordBuffer)->Enqueue(m_recordBuffer, pData, m_blockSize);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_stopFlag = true;
		m_errorString = "Error queueing buffer in callback";
	}
}

void MIPOpenSLESAndroidInput::AudioThread::staticBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
	AudioThread *pThread = reinterpret_cast<AudioThread *>(pContext);
	pThread->bufferQueueCallback(caller);
}

#endif // MIPCONFIG_SUPPORT_OPENSLESANDROID

