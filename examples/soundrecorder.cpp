/**
 * \file soundrecorder.cpp
 */

#include <mipconfig.h>

#if(defined(MIPCONFIG_SUPPORT_OSS) || defined(MIPCONFIG_SUPPORT_WINMM) || defined(MIPCONFIG_SUPPORT_PORTAUDIO))

#include <mipcomponentchain.h>
#include <mipcomponent.h>
#include <miptime.h>
#include <mipwavoutput.h>

#ifdef MIPCONFIG_SUPPORT_WINMM
	#include <mipwinmminput.h>
#else
#ifdef MIPCONFIG_SUPPORT_OSS
	#include <mipossinputoutput.h>
#else
	#include <mippainputoutput.h>
	#define NEED_PA_INIT
#endif
#endif 

#include <miprawaudiomessage.h> // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE
#include <mipsampleencoder.h>
#include <iostream>
#include <string>
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
#ifdef MIPCONFIG_SUPPORT_WINMM
	MIPWinMMInput *m_pInput;
#else
#ifdef MIPCONFIG_SUPPORT_OSS
	MIPOSSInputOutput *m_pInput;
#else
	MIPPAInputOutput *m_pInput;
#endif
#endif 
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

#ifdef MIPCONFIG_SUPPORT_WINMM
	m_pInput = new MIPWinMMInput();
	
	if (!m_pInput->open(sampRate, channels, interval, MIPTime(10.0), true)) // TODO: always use high priority?
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
#else
#ifdef MIPCONFIG_SUPPORT_OSS
	m_pInput = new MIPOSSInputOutput();
	if (!m_pInput->open(sampRate, channels, interval, MIPOSSInputOutput::ReadOnly))
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
#else
	m_pInput = new MIPPAInputOutput();
	if (!m_pInput->open(sampRate, channels, interval, MIPPAInputOutput::ReadOnly))
	{
		setErrorString(m_pInput->getErrorString());
		deleteAll();
		return false;
	}
#endif
#endif

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
#ifdef NEED_PA_INIT
	std::string errStr;

	if (!MIPPAInputOutput::initializePortAudio(errStr))
	{
		std::cerr << "Can't initialize PortAudio: " << errStr << std::endl;
		return -1;
	}
#endif // NEED_PA_INIT

	SoundRecorder sndRec;

	if (!sndRec.init("sound.wav",44100))
	{
		std::cout << sndRec.getErrorString() << std::endl;
		return -1;
	}
	getc(stdin);
	sndRec.destroy();

#ifdef NEED_PA_INIT
	MIPPAInputOutput::terminatePortAudio();
#endif // NEED_PA_INIT

	return 0;
}

#else

#include <iostream>

int main(void)
{
	std::cerr << "Not all necessary components are available to run this example." << std::endl;
	return 0;
}

#endif //
