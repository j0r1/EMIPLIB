#include "mipconfig.h"
#include <iostream>

#ifdef MIPCONFIG_SUPPORT_ALSA

#include "mipcomponentchain.h"
#include "mipaveragetimer.h"
#include "mipwavinput.h"
#include "mipalsaoutput.h"
#include "miprawaudiomessage.h" // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16
#include "mipsampleencoder.h"
#include "mipaveragetimer.h"
#include <string>
#include <stdio.h>
#include <cstdlib>

using namespace std;

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

int main(int argc, char *argv[])
{
	if (argc != 6)
	{
		cerr << "Usage: " << argv[0] << " devicename filename timerinterval outputinterval floatsamples(0/1)" << endl;
		cerr << endl;
		cerr << "Example: " << argv[0] << " plughw:0,0 soundfile.wav 0.020 0.020 0" << endl;
		cerr << endl;
		return -1;
	}
	string devName(argv[1]);
	string fileName(argv[2]);
	double timerInt = strtod(argv[3], nullptr);
	double outInt = strtod(argv[4], nullptr);
	bool floatSamples = (atoi(argv[5]) == 0)?false:true;

	std::string errStr;

	MIPTime outputInterval(outInt);
	MIPTime timerInterval(timerInt);
	MIPAverageTimer timer(timerInterval);
	MIPWAVInput sndFileInput;
	MIPAlsaOutput sndCardOutput;
	MIPSampleEncoder sampEnc;
	MyChain chain("Sound file player");
	bool returnValue;

	// We'll open the file 'soundfile.wav'

	returnValue = sndFileInput.open(fileName, outputInterval);
	checkError(returnValue, sndFileInput);
	
	// Get the parameters of the soundfile. We'll use these to initialize
	// the soundcard output component further on.

	int samplingRate = sndFileInput.getSamplingRate();
	int numChannels = sndFileInput.getNumberOfChannels();

	if (floatSamples)
		returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_FLOAT);
	else
		returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16);
	checkError(returnValue, sampEnc);

	// Initialize the soundcard output
	returnValue = sndCardOutput.open(samplingRate, numChannels, outputInterval, floatSamples, devName);
	checkError(returnValue, sndCardOutput);

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

	// We'll wait until enter is pressed

	getc(stdin);

	returnValue = chain.stop();
	checkError(returnValue, chain);

	// We'll let the destructors of most components take care
	// of their de-initialization.

	sndCardOutput.close(); // In case we're using PortAudio

	return 0;
}

#else

int main(void)
{
	std::cerr << "Not all necessary components are available to run this example." << std::endl;
	return 0;
}

#endif 

