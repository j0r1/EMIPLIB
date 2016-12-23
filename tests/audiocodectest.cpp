#include "mipconfig.h"
#include <iostream>

using namespace std;

#ifdef MIPCONFIG_SUPPORT_PORTAUDIO

#include "mipcomponentchain.h"
#include "mipaveragetimer.h"
#include "mippainputoutput.h"
#include "mipsampleencoder.h"
#include "mipwavinput.h"
#include "miprawaudiomessage.h"
#include "mipsamplingrateconverter.h"
#include "mipalawencoder.h"
#include "mipalawdecoder.h"
#include "mipulawencoder.h"
#include "mipulawdecoder.h"
#include "mipgsmencoder.h"
#include "mipgsmdecoder.h"
#include "miplpcencoder.h"
#include "miplpcdecoder.h"
#include "mipopusencoder.h"
#include "mipopusdecoder.h"
#include "mipspeexencoder.h"
#include "mipspeexdecoder.h"
#include <stdlib.h>

void checkError(bool returnValue, const MIPComponent &component)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in component: " << component.getComponentName() << std::endl;
	std::cerr << "Error description: " << component.getErrorString() << std::endl;

	exit(-1); 
}

void checkError(bool returnValue, const MIPComponentChain &chain)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in chain: " << chain.getName() << std::endl;
	std::cerr << "Error description: " << chain.getErrorString() << std::endl;

	exit(-1);
}

class MyChain : public MIPComponentChain
{
public:
	MyChain(const std::string &chainName) : MIPComponentChain(chainName)
	{
	}
private:
	void onThreadExit(bool error, const std::string &errorComponent, const std::string &errorDescription)
	{
		if (!error)
			return;

		std::cerr << "An error occured in the background thread." << std::endl;
		std::cerr << "    Component: " << errorComponent << std::endl;
		std::cerr << "    Error description: " << errorDescription << std::endl;
	}	
};

void getALawCodec(MIPComponent **pEncoder, MIPComponent **pDecoder, MIPSampleEncoder *sampEnc, MIPTime &interval, int &rate)
{
	cerr << "Using A-Law" << endl;
	MIPALawEncoder *pEnc = new MIPALawEncoder();
	MIPALawDecoder *pDec = new MIPALawDecoder();

	checkError(pEnc->init(), *pEnc);
	checkError(pDec->init(), *pDec);
	checkError(sampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16), *sampEnc);

	*pEncoder = pEnc;
	*pDecoder = pDec;
}

void getULawCodec(MIPComponent **pEncoder, MIPComponent **pDecoder, MIPSampleEncoder *sampEnc, MIPTime &interval, int &rate)
{
	cerr << "Using u-Law" << endl;
	MIPULawEncoder *pEnc = new MIPULawEncoder();
	MIPULawDecoder *pDec = new MIPULawDecoder();

	checkError(pEnc->init(), *pEnc);
	checkError(pDec->init(), *pDec);
	checkError(sampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16), *sampEnc);

	*pEncoder = pEnc;
	*pDecoder = pDec;
}

void getGSMCodec(MIPComponent **pEncoder, MIPComponent **pDecoder, MIPSampleEncoder *sampEnc, MIPTime &interval, int &rate)
{
#ifdef MIPCONFIG_SUPPORT_GSM
	cerr << "Using GSM" << endl;
	MIPGSMEncoder *pEnc = new MIPGSMEncoder();
	MIPGSMDecoder *pDec = new MIPGSMDecoder();

	checkError(pEnc->init(), *pEnc);
	checkError(pDec->init(), *pDec);
	checkError(sampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16), *sampEnc);

	*pEncoder = pEnc;
	*pDecoder = pDec;

	interval = MIPTime(0.020);
	rate = 8000;
#else
	cerr << "ERROR: GSM support was not compiled in" << endl;
#endif // MIPCONFIG_SUPPORT_GSM 
}

void getLPCCodec(MIPComponent **pEncoder, MIPComponent **pDecoder, MIPSampleEncoder *sampEnc, MIPTime &interval, int &rate)
{
#ifdef MIPCONFIG_SUPPORT_LPC
	cerr << "Using LPC" << endl;
	MIPLPCEncoder *pEnc = new MIPLPCEncoder();
	MIPLPCDecoder *pDec = new MIPLPCDecoder();

	checkError(pEnc->init(), *pEnc);
	checkError(pDec->init(), *pDec);
	checkError(sampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16), *sampEnc);

	*pEncoder = pEnc;
	*pDecoder = pDec;

	interval = MIPTime(0.020);
	rate = 8000;
#else
	cerr << "ERROR: LPC support was not compiled in" << endl;
#endif // MIPCONFIG_SUPPORT_LPC
}

void getOpusCodec(MIPComponent **pEncoder, MIPComponent **pDecoder, MIPSampleEncoder *sampEnc, MIPTime &interval, int &rate)
{
#ifdef MIPCONFIG_SUPPORT_OPUS
	cerr << "Using Opus" << endl;
	MIPOpusEncoder *pEnc = new MIPOpusEncoder();
	MIPOpusDecoder *pDec = new MIPOpusDecoder();

	checkError(pEnc->init(rate, 1, MIPOpusEncoder::Audio, MIPTime(0.020)), *pEnc);
	checkError(pDec->init(rate, 1, false), *pDec);
	checkError(sampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16), *sampEnc);

	*pEncoder = pEnc;
	*pDecoder = pDec;

	interval = MIPTime(0.020);
#else
	cerr << "ERROR: Opus support was not compiled in" << endl;
#endif // MIPCONFIG_SUPPORT_OPUS
}

void getSpeexCodec(MIPComponent **pEncoder, MIPComponent **pDecoder, MIPSampleEncoder *sampEnc, MIPTime &interval, int &rate)
{
#ifdef MIPCONFIG_SUPPORT_SPEEX
	cerr << "Using Speex" << endl;
	MIPSpeexEncoder *pEnc = new MIPSpeexEncoder();
	MIPSpeexDecoder *pDec = new MIPSpeexDecoder();

	checkError(pEnc->init(), *pEnc);
	checkError(pDec->init(), *pDec);
	checkError(sampEnc->init(MIPRAWAUDIOMESSAGE_TYPE_S16), *sampEnc);

	*pEncoder = pEnc;
	*pDecoder = pDec;

	interval = MIPTime(0.020);
	rate = 16000; // speex encoder default is wideband
#else
	cerr << "ERROR: Speex support was not compiled in" << endl;
#endif // MIPCONFIG_SUPPORT_SPEEX
}


int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		cerr << "Usage: " << argv[0] << " soundfilename.wav" << endl;
		return -1;
	}

	std::string errStr;

	if (!MIPPAInputOutput::initializePortAudio(errStr))
	{
		std::cerr << "Can't initialize PortAudio: " << errStr << std::endl;
		return -1;
	}

	MyChain chain("Audio codec test");
	MIPTime interval(0.050);
	MIPWAVInput sndFileInput;
	MIPComponent *pEncoder = nullptr;
	MIPComponent *pDecoder = nullptr;
	MIPSamplingRateConverter sampConv;
	MIPSampleEncoder sampEnc, sampEnc2, sampEnc3;
	MIPPAInputOutput sndCardOutput;

	checkError(sndFileInput.open(argv[1], interval), sndFileInput);
	int samplingRate = sndFileInput.getSamplingRate();
	int numChannels = sndFileInput.getNumberOfChannels();
	sndFileInput.close(); // we'll re-open it later with the adjusted interval

	checkError(sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16), sampEnc);
	checkError(sampEnc3.init(MIPRAWAUDIOMESSAGE_TYPE_S16), sampEnc3);

	int outputRate = samplingRate;

	// Init sample encoder, encoder, decoder
	while (!pEncoder)
	{
		cout << R"XYZ(
Enter encoder/decoder to test:

 1) A-Law
 2) u-Law
 3) GSM
 4) LPC
 5) Opus
 6) Speex

)XYZ" << endl;
		int choice = 0;

		cin >> choice;

		if (choice == 1)
			getALawCodec(&pEncoder, &pDecoder, &sampEnc2, interval, outputRate);
		else if (choice == 2)
			getULawCodec(&pEncoder, &pDecoder, &sampEnc2, interval, outputRate);
		else if (choice == 3)
			getGSMCodec(&pEncoder, &pDecoder, &sampEnc2, interval, outputRate);
		else if (choice == 4)
			getLPCCodec(&pEncoder, &pDecoder, &sampEnc2, interval, outputRate);
		else if (choice == 5)
			getOpusCodec(&pEncoder, &pDecoder, &sampEnc2, interval, outputRate);
		else if (choice == 6)
			getSpeexCodec(&pEncoder, &pDecoder, &sampEnc2, interval, outputRate);
		else
			cerr << "ERROR: invalid choice" << endl;
	}

	// The interval was possibly adjusted, reopen the file
	checkError(sndFileInput.open(argv[1], interval), sndFileInput);
	checkError(sampConv.init(outputRate, 1, false), sampConv);
	checkError(sndCardOutput.open(outputRate, 1, interval), sndCardOutput);

	MIPAverageTimer timer(interval);

	checkError(chain.setChainStart(&timer), chain);
	checkError(chain.addConnection(&timer, &sndFileInput), chain);
	checkError(chain.addConnection(&sndFileInput, &sampEnc), chain);
	checkError(chain.addConnection(&sampEnc, &sampConv), chain);
	checkError(chain.addConnection(&sampConv, &sampEnc2), chain);
	checkError(chain.addConnection(&sampEnc2, pEncoder), chain);
	checkError(chain.addConnection(pEncoder, pDecoder), chain);
	checkError(chain.addConnection(pDecoder, &sampEnc3), chain);
	checkError(chain.addConnection(&sampEnc3, &sndCardOutput), chain);

	checkError(chain.start(), chain);

	cout << "Type something to exit" << endl;
	string dummy;
	cin >> dummy;

	checkError(chain.stop(), chain);

	delete pEncoder;
	delete pDecoder;

	MIPPAInputOutput::terminatePortAudio();
	return 0;
}

#else

int main(void)
{
	cerr << "Not all necessary components are available to run this example." << endl;
	return -1;
}

#endif // MIPCONFIG_SUPPORT_PORTAUDIO

