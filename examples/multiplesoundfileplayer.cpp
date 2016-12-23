/**
 * \file multiplesoundfileplayer.cpp
 */

// This file illustrates the use of a MIPComponentAlias. Please also read
// the documentation of this component.

#include <mipconfig.h>

#if defined(WIN32) || defined(_WIN32_WCE) || defined(MIPCONFIG_SUPPORT_OSS) || defined(MIPCONFIG_SUPPORT_PORTAUDIO)

#include <mipcomponentchain.h>
#include <mipcomponent.h>
#include <miptime.h>
#include <mipaveragetimer.h>
#include <mipwavinput.h>
#include <mipsampleencoder.h>
#ifndef WIN32
#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	#define NEED_PA_INIT
	#include <mippainputoutput.h>
#else
	#include <mipossinputoutput.h>
#endif
#else
	#include <mipwinmmoutput.h>
#endif 
#include <miprawaudiomessage.h> // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE
#include <mipaudiomixer.h>
#include <mipsamplingrateconverter.h>
#include <mipcomponentalias.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstdlib>

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

	MIPTime interval(1.000); // one second intervals
	MIPAverageTimer timer(interval);
	MIPWAVInput sndFileInput1;
	MIPComponentAlias alias(&sndFileInput1);
	MIPWAVInput sndFileInput2;
	MIPSamplingRateConverter sampConv;
	MIPAudioMixer mixer;
	MIPSampleEncoder sampEnc;
#ifndef WIN32
#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	MIPPAInputOutput sndCardOutput;
#else
	MIPOSSInputOutput sndCardOutput;
#endif
#else
	MIPWinMMOutput sndCardOutput;
#endif
	MyChain chain("Sound file mixer");
	bool returnValue;
	int choice;

	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << "This example illustrates the MIPComponentAlias component, so please also read" << std::endl;
	std::cout << "the documentation of this component." << std::endl;
	std::cout << std::endl;
	std::cout << "For this example, you will need two soundfiles: one named \"soundfile-16000-mono.wav\"," << std::endl;
	std::cout << "and one named \"soundfile-8000-mono.wav\". As the names suggest, the first file" << std::endl;
	std::cout << "should use a 16000 Hz sampling rate; the second one an 8000 Hz sampling rate." << std::endl;
	std::cout << "Both files should contain mono sound, not stereo. " << std::endl;
	std::cout << std::endl;
	std::cout << "For the effect to be obvious, its best that one file is simply a resampled version" << std::endl;
	std::cout << "of the other one." << std::endl;
	std::cout << std::endl;
	std::cout << "Enter chain choice: " << std::endl;
	std::cout << "(1) Always use a sampling rate converter" << std::endl;
	std::cout << "(2) Using a MIPComponentAlias in one subchain" << std::endl;
	std::cout << "(3) Bad chain which causes strange sync problem" << std::endl;
	std::cout << std::endl;
	std::cout << "Note that in cases (1) and (2), you should here exactly the same thing. If you follow" << std::endl;
	std::cout << "the suggestion above and use one soundfile that is a resampled version of the other" << std::endl;
	std::cout << "one, you'll simply hear the sound in that file (but somewhat louder). In case (3) on" << std::endl;
	std::cout << "the other hand, the two streams are not synchronized and you will hear an offset of" << std::endl;
	std::cout << "one second on one of the streams." << std::endl;
	std::cout << std::endl;
	std::cout << "Your choice: ";
	std::cin >> choice;
	std::cout << std::endl;

	// We'll open the file 'soundfile.wav'

	returnValue = sndFileInput1.open("soundfile-16000-mono.wav", interval);
	checkError(returnValue, sndFileInput1);

	returnValue = sndFileInput2.open("soundfile-8000-mono.wav", interval);
	checkError(returnValue, sndFileInput2);

	// Get the parameters of the soundfile. We'll use these to initialize
	// the soundcard output component further on.

	int samplingRate = 16000;
	int numChannels = 1;

	// Initialize the sampling rate converter
	returnValue = sampConv.init(samplingRate, numChannels);
	checkError(returnValue, sampConv);

	// Initialize the mixer
	returnValue = mixer.init(samplingRate, numChannels, interval, false);
	checkError(returnValue, mixer);

	// Initialize the soundcard output
	returnValue = sndCardOutput.open(samplingRate, numChannels, interval);
	checkError(returnValue, sndCardOutput);

	// Initialize the sample encoder
#ifndef WIN32
#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	// The PortAudio output component uses signed 16 bit samples
	returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16);
#else
	// The OSS component can use several encoding types. We'll ask
	// the component to which format samples should be converted.
	returnValue = sampEnc.init(sndCardOutput.getRawAudioSubtype());
#endif
#else
	// The WinMM output component uses signed little endian 16 bit samples.
	returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16LE);
#endif
	checkError(returnValue, sampEnc);

	// Next, we'll create the chain

	if (choice == 1)
	{
		// Layer 1:
		//	MIPAverageTimer (0xbfdbbc7c) -> MIPWAVInput (0xbfdbbc28)
		//	MIPAverageTimer (0xbfdbbc7c) -> MIPWAVInput (0xbfdbbbd4)
		// Layer 2:
		//	MIPWAVInput (0xbfdbbc28) -> MIPSamplingRateConverter (0xbfdbbcd0)
		//	MIPWAVInput (0xbfdbbbd4) -> MIPSamplingRateConverter (0xbfdbbcd0)
		// Layer 3:
		//	MIPSamplingRateConverter (0xbfdbbcd0) -> MIPAudioMixer (0xbfdbbb24)
		// Layer 4:
		//	MIPAudioMixer (0xbfdbbb24) -> MIPSampleEncoder (0xbfdbbd1c)
		// Layer 5:
		//	MIPSampleEncoder (0xbfdbbd1c) -> MIPOSSInputOutput (0xbfdbb8f8)

		returnValue = chain.setChainStart(&timer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&timer, &sndFileInput1);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&timer, &sndFileInput2);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sndFileInput1, &sampConv);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sndFileInput2, &sampConv);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sampConv, &mixer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&mixer, &sampEnc);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sampEnc, &sndCardOutput);
		checkError(returnValue, chain);
	}
	else if (choice == 2)
	{
		// Layer 1:
		// 	MIPAverageTimer (0xbfbf84bc) -> MIPWAVInput (0xbfbf8468)
		// 	MIPAverageTimer (0xbfbf84bc) -> MIPWAVInput (0xbfbf8414)
		// Layer 2:
		// 	MIPWAVInput (0xbfbf8468) -> MIPWAVInput-alias (0xbfbf85a0)
		// 	MIPWAVInput (0xbfbf8414) -> MIPSamplingRateConverter (0xbfbf8510)
		// Layer 3:
		// 	MIPWAVInput-alias (0xbfbf85a0) -> MIPAudioMixer (0xbfbf8364)
		// 	MIPSamplingRateConverter (0xbfbf8510) -> MIPAudioMixer (0xbfbf8364)
		// Layer 4:
		// 	MIPAudioMixer (0xbfbf8364) -> MIPSampleEncoder (0xbfbf855c)
		// Layer 5:
		// 	MIPSampleEncoder (0xbfbf855c) -> MIPOSSInputOutput (0xbfbf8138)

		returnValue = chain.setChainStart(&timer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&timer, &sndFileInput1);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&timer, &sndFileInput2);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sndFileInput1, &alias, false, 0, 0);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sndFileInput2, &sampConv);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sampConv, &mixer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&alias, &mixer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&mixer, &sampEnc);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sampEnc, &sndCardOutput);
		checkError(returnValue, chain);
	}
	else if (choice == 3)
	{
		// Layer 1:
		// 	MIPAverageTimer (0xbfe8133c) -> MIPWAVInput (0xbfe812e8)
		// 	MIPAverageTimer (0xbfe8133c) -> MIPWAVInput (0xbfe81294)
		// Layer 2:
		// 	MIPWAVInput (0xbfe812e8) -> MIPAudioMixer (0xbfe811e4)
		// 	MIPWAVInput (0xbfe81294) -> MIPSamplingRateConverter (0xbfe81390)
		// Layer 3:
		// 	MIPAudioMixer (0xbfe811e4) -> MIPSampleEncoder (0xbfe813dc)
		// 	MIPSamplingRateConverter (0xbfe81390) -> MIPAudioMixer (0xbfe811e4)
		// Layer 4:
		// 	MIPSampleEncoder (0xbfe813dc) -> MIPOSSInputOutput (0xbfe80fb8)

		returnValue = chain.setChainStart(&timer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&timer, &sndFileInput1);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&timer, &sndFileInput2);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sndFileInput1, &mixer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sndFileInput2, &sampConv);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sampConv, &mixer);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&mixer, &sampEnc);
		checkError(returnValue, chain);

		returnValue = chain.addConnection(&sampEnc, &sndCardOutput);
		checkError(returnValue, chain);
	}
	else
	{
		std::cerr << "Invalid choice" << std::endl;
		return -1;
	}

	// Start the chain

	returnValue = chain.start();
	checkError(returnValue, chain);

	// We'll wait until enter is pressed

	std::string str;

	std::cout << "Type something to quit" << std::endl;
	std::cin >> str;

	returnValue = chain.stop();
	checkError(returnValue, chain);

	// We'll let the destructors of the components take care
	// of their de-initialization.

	sndCardOutput.close(); // In case we're using PortAudio
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

#endif // WIN32 || _WIN32_WCE || MIPCONFIG_SUPPORT_OSS

