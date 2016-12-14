/**
 * \file soundrecorder.cpp
 */

#include <mipconfig.h>
#include <mipcomponentchain.h>
#include <mipcomponent.h>
#include <miptime.h>
#include <mipwavoutput.h>

#if (defined(WIN32) || defined(_WIN32_WCE))
	#include <mipwinmminput.h>
#else
	#include <mipossinputoutput.h>
#endif // WIN32 || _WIN32_WCE

#include <miprawaudiomessage.h> // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE
#include <mipsampleencoder.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <stdio.h>

class SoundRecorder : public MIPErrorBase
{
public:
	SoundRecorder();
	virtual ~SoundRecorder();

	bool init(const std::string &fname, int sampRate);

	bool destroy();
protected:
	virtual void onThreadExit(bool err, const std::string &compName, const std::string &errStr)		{ }
private:
	class SoundRecorderChain : public MIPComponentChain
	{
	public:
		SoundRecorderChain(SoundRecorder *pSndRec) : MIPComponentChain("Sound Recorder Chain")		{ m_pSndRec = pSndRec; }
	protected:
		void onThreadExit(bool err, const std::string &compName, const std::string &errStr)		{ m_pSndRec->onThreadExit(err, compName, errStr); }
	private:
		SoundRecorder *m_pSndRec;
	};

	void zeroAll();
	void deleteAll();

	bool m_init;

	SoundRecorderChain *m_pChain;
#if (defined(WIN32) || defined(_WIN32_WCE))
	MIPWinMMInput *m_pInput;
#else
	MIPOSSInputOutput *m_pInput;
#endif // WIN32 || _WIN32_WCE
	MIPSampleEncoder *m_pSampEnc;
	MIPWAVOutput *m_pOutput;

	friend class SoundRecorderChain;
};

SoundRecorder::SoundRecorder()
{
	zeroAll();
	m_init = false;
}

SoundRecorder::~SoundRecorder()
{
	deleteAll();
}

bool SoundRecorder::init(const std::string &fname, int sampRate)
{
	if (m_init)
	{
		setErrorString("Already initialized");
		return false;
	}

	MIPTime interval(0.200); // We'll use 200 ms intervals
	int channels = 1;

#if (defined(WIN32) || defined(_WIN32_WCE))
	m_pInput = new MIPWinMMInput();
	
	if (!m_pInput->open(sampRate, channels, interval, MIPTime(10.0), true)) // TODO: always use high priority?
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
#else
	m_pInput = new MIPOSSInputOutput();
	if (!m_pInput->open(sampRate, channels, interval, MIPOSSInputOutput::ReadOnly))
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
#endif // WIN32 || _WIN32_WCE

	m_pSampEnc = new MIPSampleEncoder();
	if (!m_pSampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_U8))
	{
		setErrorString(m_pSampEnc->getErrorString());
		deleteAll();
		return false;
	}

	m_pOutput = new MIPWAVOutput();
	if (!m_pOutput->open(fname, sampRate))
	{
		setErrorString(m_pOutput->getErrorString());
		deleteAll();
		return false;
	}

	m_pChain = new SoundRecorderChain(this);
	m_pChain->setChainStart(m_pInput);
	m_pChain->addConnection(m_pInput, m_pSampEnc);
	m_pChain->addConnection(m_pSampEnc, m_pOutput);

	if (!m_pChain->start())
	{
		setErrorString(m_pChain->getErrorString());
		deleteAll();
		return false;
	}

	m_init = true;
	return true;
}

bool SoundRecorder::destroy()
{
	if (!m_init)
	{
		setErrorString("Not initialized");
		return false;
	}

	deleteAll();
	m_init = false;
	return true;
}

void SoundRecorder::zeroAll()
{
	m_pChain = 0;
	m_pInput = 0;
	m_pSampEnc = 0;
	m_pOutput = 0;
}

void SoundRecorder::deleteAll()
{
	if (m_pChain)
	{
		m_pChain->stop();
		delete m_pChain;
	}
	if (m_pInput)
		delete m_pInput;
	if (m_pSampEnc)
		delete m_pSampEnc;
	if (m_pOutput)
		delete m_pOutput;
	zeroAll();
}

int main(void)
{
	SoundRecorder sndRec;

	if (!sndRec.init("sound.wav",44100))
	{
		std::cout << sndRec.getErrorString() << std::endl;
		return -1;
	}
	getc(stdin);
	sndRec.destroy();

	return 0;
}

