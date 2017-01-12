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

#include "mipqt5audiooutput.h"

#ifdef MIPCONFIG_SUPPORT_QT5

#include "mipstreambuffer.h"
#include "miprawaudiomessage.h"
#include <QIODevice>
#include <QAudioOutput>
#include <iostream>

using namespace std;

#define MIPQT5AUDIOOUTPUT_ERRSTR_NOTINITIALIZED	"Qt5 audio output component is not initialized"
#define MIPQT5AUDIOOUTPUT_ERRSTR_ALREADYINITIALIZED	"Qt5 audio output component is already initialized"
#define MIPQT5AUDIOOUTPUT_ERRSTR_NOPULL "The Qt5 audio output component does not support the pull method"
#define MIPQT5AUDIOOUTPUT_ERRSTR_BADMESSAGE "For the Qt5 audio output component, audio messages must be signed 16 bit native endian raw audio messages"
#define MIPQT5AUDIOOUTPUT_ERRSTR_INCOMPATSAMPLINGRATE "The sampling rate of an incoming message is not the same as the one specified during initialization"
#define MIPQT5AUDIOOUTPUT_ERRSTR_INCOMPATCHANNELS "The number of channels in an incoming message is not the same as the one specified during initialization"
#define MIPQT5AUDIOOUTPUT_ERRSTR_BADSAMPLINGRATE "The sampling rate must be positive"
#define MIPQT5AUDIOOUTPUT_ERRSTR_BADCHANNELS "The number of channels must be positive"
#define MIPQT5AUDIOOUTPUT_ERRSTR_BADINTERVAL "The output interval is too small, must be at least 1ms"
#define MIPQT5AUDIOOUTPUT_ERRSTR_BADQUEUEPARAM "The maximum number of queued buffers must be at least one"
#define MIPQT5AUDIOOUTPUT_ERRSTR_CANTINITIODEVICE "Unable to initialize the QIODevice"
#define MIPQT5AUDIOOUTPUT_ERRSTR_INCOMPATAUDIODEVICE "The specified audio device does not support specified sampling rate, number of channels, or 16 bit signed samples (native endianness)"
#define MIPQT5AUDIOOUTPUT_ERRSTR_CANTFINDCOMPATIBLEDEVICE "Unable to find an audio output device that supports the specifies sampling rate, number of channels and 16 bit signed samples (native endianness)"
#define MIPQT5AUDIOOUTPUT_ERRSTR_CANTSTARTOUTPUT "Unable to start the QAudioOutput module"
#define MIPQT5AUDIOOUTPUT_ERRSTR_DETECTEDERROR "Error detected while playing back audio"

class MIPQt5AudioOutput_StreamBufferIODevice : public QIODevice
{
public:
	MIPQt5AudioOutput_StreamBufferIODevice() : QIODevice()
	{
		m_pBuffer = 0;
	}

	bool init(MIPStreamBuffer *pBuffer, int bytesPerBlock)
	{
		if (!pBuffer)
			return false;
		m_pBuffer = pBuffer;
		m_bytesPerBlock = bytesPerBlock;
		return QIODevice::open(QIODevice::ReadOnly);
	}

	qint64 readData(char *data, qint64 len)
	{
		if (!m_pBuffer)
			return -1;

		int num = m_pBuffer->read(data, len);
		if (num < m_bytesPerBlock) // not enough for a single block, add some silence to prevent underrun
		{
			int remainingLen = (int)len - num;
			int bytesToDo = m_bytesPerBlock-num;

			if (bytesToDo > remainingLen) // make sure we don't go out of bounds
				bytesToDo = remainingLen;

			memset(data + num, 0, bytesToDo);
			num += bytesToDo;
			//cerr << "Trying to prevent underrun" << endl;
		}
		//cerr << "readData " << len << " " << num << endl;
		return num;
	}

	qint64 writeData(const char *data, qint64 len)
	{
		return 0;
	}

	qint64 bytesAvailable() const
	{
		if (!m_pBuffer)
			return -1;
		int avail = m_pBuffer->getAmountBuffered();
		//cerr << "bytesAvailable = " << avail << endl;
		return avail;
	}
private:
	MIPStreamBuffer *m_pBuffer;
	int m_bytesPerBlock;
};

MIPQt5AudioOutput::MIPQt5AudioOutput() : MIPComponent("MIPQt5AudioOutput")
{
	m_init = false;
	m_pBuffer = 0;
}

MIPQt5AudioOutput::~MIPQt5AudioOutput()
{
	close();
}

bool MIPQt5AudioOutput::open(int samplingRate, int numChannels, MIPTime interval, const QAudioDeviceInfo *pAudioDevice,
                             int maxQueuedBuffers)
{
	if (m_init)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_ALREADYINITIALIZED);
		return false;
	}

	if (samplingRate < 1)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_BADSAMPLINGRATE);
		return false;
	}

	if (numChannels < 1)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_BADCHANNELS);
		return false;
	}

	m_sampRate = samplingRate;
	m_numChannels = numChannels;
	
	double dt = interval.getValue();
	if (dt < 0.001)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_BADINTERVAL);
		return false;
	}

	if (maxQueuedBuffers < 1)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_BADQUEUEPARAM);
		return false;
	}

	m_maxQueuedBuffers = maxQueuedBuffers;
	m_bytesPerFrame = (int)((double)samplingRate*dt + 0.5) * sizeof(int16_t) * numChannels;	
	//cout << "Bytes per frame = " << m_bytesPerFrame << endl;

	QAudioFormat fmt;

	fmt.setChannelCount(numChannels);
	fmt.setSampleRate(samplingRate);
	fmt.setSampleSize(sizeof(int16_t)*8);
	fmt.setSampleType(QAudioFormat::SignedInt);
	fmt.setCodec("audio/pcm");

	QAudioDeviceInfo devInfo;
	if (pAudioDevice)
	{
		devInfo = *pAudioDevice;
		if (!devInfo.isFormatSupported(fmt))
		{
			setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_INCOMPATAUDIODEVICE);
			return false;
		}
	}
	else
	{
		auto devs = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
		bool gotDevice = false;

		for (auto &d: devs)
		{
			//cerr << "Checking " << d.deviceName().toStdString() << endl;
			if (d.isFormatSupported(fmt))
			{
				devInfo = d;
				gotDevice = true;
				break;
			}
		}

		if (!gotDevice)
		{
			setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_CANTFINDCOMPATIBLEDEVICE);
			return false;
		}
	}

	m_pBuffer = new MIPStreamBuffer(m_bytesPerFrame, 10);
	MIPQt5AudioOutput_StreamBufferIODevice *pIODev = new MIPQt5AudioOutput_StreamBufferIODevice();
	m_pIODev = pIODev;
	if (!pIODev->init(m_pBuffer, m_bytesPerFrame))
	{
		delete pIODev;
		delete m_pBuffer;
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_CANTINITIODEVICE);
		return false;
	}

	m_pAudioOutput = new QAudioOutput(devInfo, fmt);
	m_pAudioOutput->setBufferSize(m_bytesPerFrame);
	
	m_pAudioOutput->start(m_pIODev);
	if (m_pAudioOutput->error() != QAudio::NoError)
	{
		delete m_pAudioOutput;
		delete pIODev;
		delete m_pBuffer;
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_CANTSTARTOUTPUT);
		return false;
	}

	m_init = true;

	return true;
}

bool MIPQt5AudioOutput::close()
{
	if (!m_init)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_NOTINITIALIZED);
		return false;
	}

	delete m_pAudioOutput;
	delete m_pIODev;
	delete m_pBuffer;

	m_init = false;

	return true;
}

bool MIPQt5AudioOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_NOTINITIALIZED);
		return false;
	}
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
	      pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 ))
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPAudioMessage *audioMessage = (MIPAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_INCOMPATSAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_numChannels)
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_INCOMPATCHANNELS);
		return false;
	}

	size_t num = audioMessage->getNumberOfFrames();

	MIPRawFloatAudioMessage *audioMessageFloat = (MIPRawFloatAudioMessage *)pMsg;
	const float *pFrames = audioMessageFloat->getFrames();

	if (m_pBuffer->getAmountBuffered() > m_bytesPerFrame * sizeof(int16_t) * m_maxQueuedBuffers)
	{
		m_pBuffer->clear();
		//cerr << "Data buildup, resetting buffer" << endl;
	}

	m_pBuffer->write(pFrames, sizeof(int16_t)*num*m_numChannels);

	if (m_pAudioOutput->error() != QAudio::NoError) // Some kind of error, try resetting the device
	{
		setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_DETECTEDERROR);
		return false;
	}
	return true;
}

bool MIPQt5AudioOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPQT5AUDIOOUTPUT_ERRSTR_NOPULL);
	return false;
}

#endif // MIPCONFIG_SUPPORT_QT5

