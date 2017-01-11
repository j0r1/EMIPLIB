#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_QT5

#include "mipcomponentchain.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipaveragetimer.h"
#include "mipwavinput.h"
#include "mipsampleencoder.h"
#include "miprawaudiomessage.h" // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE
#include "mipstreambuffer.h"
#include <QAudioOutput>
#include <QIODevice>
#include <QApplication>
#include <QWindow>
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstdlib>

using namespace std;

void checkError(bool returnValue, const MIPComponent &component)
{
	if (returnValue == true)
		return;

	cerr << "An error occured in component: " << component.getComponentName() << endl;
	cerr << "Error description: " << component.getErrorString() << endl;

	exit(-1);
}

void checkError(bool returnValue, const MIPComponentChain &chain)
{
	if (returnValue == true)
		return;

	cerr << "An error occured in chain: " << chain.getName() << endl;
	cerr << "Error description: " << chain.getErrorString() << endl;

	exit(-1);
}

class MyChain : public MIPComponentChain
{
public:
	MyChain(const string &chainName) : MIPComponentChain(chainName)
	{
	}
private:
	void onThreadExit(bool error, const string &errorComponent, const string &errorDescription)
	{
		if (!error)
			return;

		cerr << "An error occured in the background thread." << endl;
		cerr << "    Component: " << errorComponent << endl;
		cerr << "    Error description: " << errorDescription << endl;
	}	
};

class MyOutput : public MIPComponent
{
public:
	MyOutput() : MIPComponent("MyOutput")
	{
		m_pBuffer = 0;
	}

	~MyOutput()
	{
		delete m_pBuffer;
	}

	MIPStreamBuffer *getBuffer() { return m_pBuffer; }

	bool init(int samplingRate, int numChannels, MIPTime interval)
	{
		// TODO: 
		if (m_pBuffer)
		{
			setErrorString("Already initialized");
			return false;
		}
		
		if (samplingRate < 8000 || samplingRate > 48000)
		{
			setErrorString("Bad sampling rate");
			return false;
		}

		if (numChannels < 1 || numChannels > 2)
		{
			setErrorString("Bad number of channels");
			return false;
		}

		m_sampRate = samplingRate;
		m_numChannels = numChannels;
		
		double dt = interval.getValue();
		if (dt < 0.005 || dt > 1.0)
		{
			setErrorString("Bad sampling interval");
			return false;
		}

		m_bytesPerFrame = (int)((double)samplingRate*dt + 0.5) * sizeof(int16_t) * numChannels;
		cout << "Bytes per frame = " << m_bytesPerFrame << endl;

		m_pBuffer = new MIPStreamBuffer(m_bytesPerFrame, 10);
		return true;
	}

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
	{
		if (!m_pBuffer)
		{
			setErrorString("Not initialized");
			return false;
		}
		if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
			pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16 ))
		{
			setErrorString("Bad message");
			return false;
		}

		MIPAudioMessage *audioMessage = (MIPAudioMessage *)pMsg;
		
		if (audioMessage->getSamplingRate() != m_sampRate)
		{
			setErrorString("Incompatible sampling rate");
			return false;
		}
		if (audioMessage->getNumberOfChannels() != m_numChannels)
		{
			setErrorString("Incompatible number of channels");
			return false;
		}
	
		size_t num = audioMessage->getNumberOfFrames();

		MIPRawFloatAudioMessage *audioMessageFloat = (MIPRawFloatAudioMessage *)pMsg;
		const float *pFrames = audioMessageFloat->getFrames();

		if (m_pBuffer->getAmountBuffered() > m_bytesPerFrame * sizeof(int16_t))
		{
			m_pBuffer->clear();
			cerr << "Data buildup, resetting buffer" << endl;
		}

		m_pBuffer->write(pFrames, sizeof(int16_t)*num*m_numChannels);

		return true;
	}

	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
	{
		setErrorString("Pull not implemented");
		return false;
	}
private:
	MIPStreamBuffer *m_pBuffer;
	int m_numChannels, m_sampRate, m_bytesPerFrame;
};

class StreamBufferIODevice : public QIODevice
{
public:
	StreamBufferIODevice() : QIODevice()
	{
		m_pBuffer = 0;
	}

	bool init(MIPStreamBuffer *pBuffer)
	{
		if (!pBuffer)
			return false;
		m_pBuffer = pBuffer;
		return QIODevice::open(QIODevice::ReadOnly);
	}

	qint64 readData(char *data, qint64 len)
	{
		if (!m_pBuffer)
			return -1;

		int num = m_pBuffer->read(data, len);
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
		cerr << "bytesAvailable = " << avail << endl;
		return avail;
	}
private:
	MIPStreamBuffer *m_pBuffer;
};

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	MIPTime interval(0.020);
	MIPAverageTimer timer(interval);
	MIPWAVInput sndFileInput;
	MIPSampleEncoder sampEnc;
	MyChain chain("Sound file player");
	MyOutput sndCardOutput;
	QAudioOutput qtAudioOutput;
	StreamBufferIODevice ioDev;
	bool returnValue;

	// We'll open the file 'soundfile.wav'

	returnValue = sndFileInput.open("soundfile.wav", interval);
	checkError(returnValue, sndFileInput);
	
	// Get the parameters of the soundfile. We'll use these to initialize
	// the soundcard output component further on.

	int samplingRate = sndFileInput.getSamplingRate();
	int numChannels = sndFileInput.getNumberOfChannels();

	cout << "Sampling rate is " << samplingRate << endl;
	cout << "Number of channels is " << numChannels << endl;

	// Initialize the soundcard output
	returnValue = sndCardOutput.init(samplingRate, numChannels, interval);
	checkError(returnValue, sndCardOutput);

	ioDev.init(sndCardOutput.getBuffer());

	returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16);
	checkError(returnValue, sampEnc);

	// Next, we'll create the chain
	returnValue = chain.setChainStart(&timer);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&timer, &sndFileInput);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sndFileInput, &sampEnc);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sampEnc, &sndCardOutput);
	checkError(returnValue, chain);

	// Start the chain

	returnValue = chain.start();
	checkError(returnValue, chain);

	QAudioFormat fmt;

	fmt.setChannelCount(numChannels);
	fmt.setSampleRate(samplingRate);
	fmt.setSampleSize(sizeof(int16_t)*8);
	fmt.setSampleType(QAudioFormat::SignedInt);
	fmt.setByteOrder(QAudioFormat::LittleEndian);
	fmt.setCodec("audio/pcm");

	auto devs = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
	QAudioDeviceInfo device;
	bool gotDevice = false;

	for (auto &d: devs)
	{
		cerr << "Checking " << d.deviceName().toStdString() << endl;
		if (d.isFormatSupported(fmt))
		{
			device = d;
			gotDevice = true;
			break;
		}
	}

	if (!gotDevice)
	{
		cerr << "No device found" << endl;
	}
	else
	{
		cerr << "Using device " << device.deviceName().toStdString() << endl;

		QAudioOutput qAudio(device, fmt);
		qAudio.setBufferSize(numChannels*samplingRate*sizeof(int16_t)*interval.getValue());

		qAudio.start(&ioDev);

		cerr << "error is " << qAudio.error() << endl;
		cerr << "buffer size is " << qAudio.bufferSize() << endl;

		QWindow w;
		w.show();

		app.exec();
	}

	returnValue = chain.stop();
	checkError(returnValue, chain);

	return 0;
}

#else

#include <iostream>

int main(void)
{
	cerr << "Not all necessary components are available to run this example." << endl;
	return 0;
}

#endif 

