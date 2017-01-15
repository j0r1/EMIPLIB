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

#include "mipopenslesandroidoutput.h"
#include "miprawaudiomessage.h"
#include "mipstreambuffer.h"
#include <vector>
#include <sstream>

using namespace std;

const int numBuffersInQueue = 2;

#define MIPOPENSLESANDROIDOUTPUT_ERRSTR_ALREADYINIT "The OpenSL ES audio output component is already initialized"
#define MIPOPENSLESANDROIDOUTPUT_ERRSTR_NOTINIT "The OpenSL ES audio output component is not initialized"
#define MIPOPENSLESANDROIDOUTPUT_ERRSTR_NOPULL "Pull is not supported"

MIPOpenSLESAndroidOutput::MIPOpenSLESAndroidOutput() : MIPComponent("MIPOpenSLESAndroidOutput")
{
	m_init = false;
}

MIPOpenSLESAndroidOutput::~MIPOpenSLESAndroidOutput()
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

bool MIPOpenSLESAndroidOutput::open(int sampRate, int channels, MIPTime interval)
{
	if (m_init)
	{
		setErrorString(MIPOPENSLESANDROIDOUTPUT_ERRSTR_ALREADYINIT);
		return false;
	}

	if (channels < 1 || channels > 2)
	{
		setErrorString("Unsupported number of channels");
		return false;
	}

	m_blockSize = (int)((double)sampRate*interval.getValue() + 0.5) * sizeof(int16_t) * channels;
	m_sampRate = sampRate;
	m_channels = channels;

	m_pBuffer = new MIPStreamBuffer(m_blockSize, 10);
	m_pThread = new AudioThread(m_pBuffer, m_blockSize, sampRate, channels, interval);
	if (m_pThread->Start() < 0)
	{
		delete m_pThread;
		delete m_pBuffer;
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
		return false;
	}

	m_init = true;

	return true;
}

bool MIPOpenSLESAndroidOutput::close()
{
	if (!m_init)
	{
		setErrorString(MIPOPENSLESANDROIDOUTPUT_ERRSTR_NOTINIT);
		return false;
	}

	delete m_pThread;
	delete m_pBuffer;

	m_init = false;
	return true;
}

bool MIPOpenSLESAndroidOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPOPENSLESANDROIDOUTPUT_ERRSTR_NOTINIT);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
		pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16))
	{
		setErrorString("Bad message");
		return false;
	}
	
	MIPAudioMessage *audioMessage = static_cast<MIPAudioMessage *>(pMsg);
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString("Bad sampling rate");
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString("Bad number of channels");
		return false;
	}
	
	size_t num = audioMessage->getNumberOfFrames();
	size_t blockSize = num*m_channels*sizeof(int16_t);

	if ((int)blockSize != m_blockSize)
	{
		setErrorString("Bad amount of data");
		return false;
	}

	MIPRaw16bitAudioMessage *audioMessageS16 = static_cast<MIPRaw16bitAudioMessage *>(pMsg);
	const uint16_t *pFrames = audioMessageS16->getFrames();

	if (!m_pThread->IsRunning())
	{
		setErrorString("Audio thread is no longer running: " + m_pThread->getErrorString());
		return false;
	}

	// TODO: set high priority?

	m_pBuffer->write(pFrames, blockSize);
	return true;
}

bool MIPOpenSLESAndroidOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPOPENSLESANDROIDOUTPUT_ERRSTR_NOPULL);
	return false;
}

MIPOpenSLESAndroidOutput::AudioThread::AudioThread(MIPStreamBuffer *pBuffer, int blockSize, int sampRate, int channels, MIPTime interval)
{
	m_stopFlag = false;
	m_successFullyStarted = false;

	m_pBuffer = pBuffer;
	m_blockSize = blockSize;
	m_sampRate = sampRate;
	m_channels = channels;
	m_interval = interval;

	m_empty.resize(m_blockSize, 0);
	m_buf.resize(m_blockSize, 0);
}

MIPOpenSLESAndroidOutput::AudioThread::~AudioThread()
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

void *MIPOpenSLESAndroidOutput::AudioThread::Thread()
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

	SLObjectItf outputMixObj;
	retVal = (*engine)->CreateOutputMix(engine, &outputMixObj, 0, nullptr, nullptr);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't create output mix object";
		return 0;
	}
	interfacesToDestroyOnError.add(outputMixObj);

	retVal = (*outputMixObj)->Realize(outputMixObj, SL_BOOLEAN_FALSE);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString =  "Can't realize output mix object";
		return false;
	}

	SLDataLocator_AndroidSimpleBufferQueue inputQueue;
	inputQueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
	inputQueue.numBuffers = numBuffersInQueue; // This should be about the minumum, for low latency audio

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

	SLDataSource src;
	src.pLocator = &inputQueue;
	src.pFormat = &format;

	SLDataLocator_OutputMix outMixLoc;
	outMixLoc.locatorType = SL_DATALOCATOR_OUTPUTMIX;
	outMixLoc.outputMix = outputMixObj;

	SLDataSink sink;
	sink.pLocator = &outMixLoc;
	sink.pFormat = nullptr;

	SLObjectItf playerObj;
	SLPlayItf player;
	
	const SLInterfaceID neededInterfaces[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
	const SLboolean requiredFlags[] = { SL_BOOLEAN_TRUE };

	retVal = (*engine)->CreateAudioPlayer(engine, &playerObj, &src, &sink, 1, neededInterfaces, requiredFlags);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't get audio player interface";
		return 0;
	}
	interfacesToDestroyOnError.add(playerObj);
	 
	retVal = (*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't realize player object";
		return 0;
	}
	 
	retVal = (*playerObj)->GetInterface(playerObj, SL_IID_PLAY, &player);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't get player interface";
		return 0;
	}
	 
	retVal = (*playerObj)->GetInterface(playerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &m_playerBuffer);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't get player buffer queue interface";
		return 0;
	}

	retVal = (*m_playerBuffer)->RegisterCallback(m_playerBuffer, staticBufferQueueCallback, this);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't register callback";
		return 0;
	}

	retVal = (*player)->SetPlayState(player, SL_PLAYSTATE_PLAYING);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_errorString = "Can't set player to 'playing' state";
		return 0;
	}

	// fill the buffers with silence first
	for (int i = 0 ; i < numBuffersInQueue ; i++) 
	{
		auto retVal = (*m_playerBuffer)->Enqueue(m_playerBuffer, &m_empty[0], m_empty.size());
		if (retVal != SL_RESULT_SUCCESS)
		{
			m_errorString = "Error queueing initial buffers";
			return 0;
		}
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

	(*player)->SetPlayState(player, SL_PLAYSTATE_STOPPED);

	// The destructor of interfacesToDestroyOnError will do the de-initialization

	return 0;
}

void MIPOpenSLESAndroidOutput::AudioThread::bufferQueueCallback(SLAndroidSimpleBufferQueueItf caller)
{
	uint8_t *pData = &m_buf[0];
	int buffered = m_pBuffer->getAmountBuffered();

	if (buffered >= m_blockSize)
	{
		m_pBuffer->read(&m_buf[0], m_blockSize);
		if (buffered - m_blockSize > 2*m_blockSize) // TODO: make this configurable?
			m_pBuffer->clear();
	}
	else
		pData = &m_empty[0];

	auto retVal = (*m_playerBuffer)->Enqueue(m_playerBuffer, pData, m_blockSize);
	if (retVal != SL_RESULT_SUCCESS)
	{
		m_stopFlag = true;
		m_errorString = "Error queueing buffer in callback";
	}
}

void MIPOpenSLESAndroidOutput::AudioThread::staticBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
	AudioThread *pThread = reinterpret_cast<AudioThread *>(pContext);
	pThread->bufferQueueCallback(caller);
}

#endif // MIPCONFIG_SUPPORT_OPENSLESANDROID

