/**
 * \file soundvolume.cpp
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
#include <cmath>
#include <stdio.h>

class VolumeComponent : public MIPComponent
{
public:
	VolumeComponent() : MIPComponent("VolumeComponent")
	{
		m_count = 0;
	}

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
	{
		if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
		{
			setErrorString("Not a floating point raw audio message");
			return false;
		}

		MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;
		const float *pFrames = pAudioMsg->getFrames();
		int num = pAudioMsg->getNumberOfFrames() * pAudioMsg->getNumberOfChannels();
		float maxValue = 0;

		for (int i = 0 ; i < num ; i++)
		{
			float v = std::abs(pFrames[i]);

			if (v > maxValue)
				maxValue = v;
		}

		int maxLen = 50;
		int realLen = maxLen * maxValue;

		fprintf(stderr, "\rIteration(%03d): ",m_count);
		for (int i = 0 ; i < realLen ; i++)
		{
			fprintf(stderr, "=");
		}
		for (int i = realLen ; i < maxLen ; i++)
		{
			fprintf(stderr, " ");
		}

		if (m_count == 0)
			m_startTime = MIPTime::getCurrentTime();

		if (m_count > 5)
		{
			MIPTime t = MIPTime::getCurrentTime();
			real_t timeDiff = t.getValue() - m_startTime.getValue();
			real_t avgTime = timeDiff/(real_t)m_count;
			
			fprintf(stderr, "%g%%   ", (double)avgTime/0.200*100.0);
		}

		m_count++;
		return true;
	}
	
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
	{
		setErrorString("Pull not supported");
		return false;
	}

	int m_count;
	MIPTime m_startTime;
};

class SoundVolume : public MIPErrorBase
{
public:
	SoundVolume();
	virtual ~SoundVolume();

	bool init(int sampRate);

	bool destroy();
protected:
	virtual void onThreadExit(bool err, const std::string &compName, const std::string &errStr)		{ }
private:
	class SoundVolumeChain : public MIPComponentChain
	{
	public:
		SoundVolumeChain(SoundVolume *pSndRec) : MIPComponentChain("Sound Volume Chain")		{ m_pSndRec = pSndRec; }
	protected:
		void onThreadExit(bool err, const std::string &compName, const std::string &errStr)		{ m_pSndRec->onThreadExit(err, compName, errStr); }
	private:
		SoundVolume *m_pSndRec;
	};

	void zeroAll();
	void deleteAll();

	bool m_init;

	SoundVolumeChain *m_pChain;
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
	VolumeComponent *m_pVolumeComp;

	friend class SoundVolumeChain;
};

SoundVolume::SoundVolume()
{
	zeroAll();
	m_init = false;
}

SoundVolume::~SoundVolume()
{
	deleteAll();
}

bool SoundVolume::init(int sampRate)
{
	if (m_init)
	{
		setErrorString("Already initialized");
		return false;
	}

	MIPTime interval(0.200); // We'll use 200 ms intervals
	int channels = 1;

	std::cout << "Opening soundcard" << std::endl;

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
	if (!m_pSampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
	{
		setErrorString(m_pSampEnc->getErrorString());
		deleteAll();
		return false;
	}

	m_pVolumeComp = new VolumeComponent();

	m_pChain = new SoundVolumeChain(this);
	m_pChain->setChainStart(m_pInput);
	m_pChain->addConnection(m_pInput, m_pSampEnc);
	m_pChain->addConnection(m_pSampEnc, m_pVolumeComp);

	std::cout << "Starting chain" << std::endl;

	if (!m_pChain->start())
	{
		setErrorString(m_pChain->getErrorString());
		deleteAll();
		return false;
	}

	std::cout << "Chain running" << std::endl << std::endl;

	m_init = true;
	return true;
}

bool SoundVolume::destroy()
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

void SoundVolume::zeroAll()
{
	m_pChain = 0;
	m_pInput = 0;
	m_pSampEnc = 0;
	m_pVolumeComp = 0;
}

void SoundVolume::deleteAll()
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
	if (m_pVolumeComp)
		delete m_pVolumeComp;
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

	SoundVolume sndRec;

	if (!sndRec.init(48000))
	{
		std::cerr << sndRec.getErrorString() << std::endl;
		return -1;
	}
	getc(stdin);
	sndRec.destroy();

	fprintf(stderr,"\n");

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
