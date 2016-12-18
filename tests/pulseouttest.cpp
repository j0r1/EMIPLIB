#include "mipconfig.h"
#include <iostream>

#ifdef MIPCONFIG_SUPPORT_PULSEAUDIO

#include "mipcomponentchain.h"
#include "mipaveragetimer.h"
#include "mipwavinput.h"
#include "mippulseoutput.h"
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
	MIPTime interval(0.020); // We'll use 50 millisecond intervals
	MIPAverageTimer timer(interval);
	MIPWAVInput sndFileInput;
	MIPPulseOutput sndCardOutput;
	MyChain chain("Sound file player");
	bool returnValue;

	// We'll open the file 'soundfile.wav'

	returnValue = sndFileInput.open("soundfile.wav", interval);
	checkError(returnValue, sndFileInput);
	
	// Get the parameters of the soundfile. We'll use these to initialize
	// the soundcard output component further on.

	int samplingRate = sndFileInput.getSamplingRate();
	int numChannels = sndFileInput.getNumberOfChannels();

	// Initialize the soundcard output
	returnValue = sndCardOutput.open(samplingRate, numChannels, interval);
	checkError(returnValue, sndCardOutput);

	// Next, we'll create the chain
	returnValue = chain.setChainStart(&timer);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&timer, &sndFileInput);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sndFileInput, &sndCardOutput);
	checkError(returnValue, chain);

	// Start the chain

	returnValue = chain.start();
	checkError(returnValue, chain);

	// We'll wait until enter is pressed

	getc(stdin);

	returnValue = chain.stop();
	checkError(returnValue, chain);

	// We'll let the destructors of most components take care
	// of their de-initialization.

	return 0;
}

#else

int main(void)
{
	std::cerr << "Not all necessary components are available to run this example." << std::endl;
	return 0;
}

#endif 

