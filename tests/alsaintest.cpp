#include "mipconfig.h"
#include <iostream>

#ifdef MIPCONFIG_SUPPORT_ALSA

#include "mipcomponentchain.h"
#include "mipalsainput.h"
#include "mipalsaoutput.h"
#include "mipaudiomixer.h"
#include "mipmessagedumper.h"
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
		cerr << "Usage: " << argv[0] << " devname samprate channels floatsamples(0/1) extradelay" << endl;
		cerr << endl;
		cerr << " example: " << argv[0] << " plughw:0,0 48000 2 false 2.0" << endl;
		cerr << endl;
		return -1;
	}

	string devName(argv[1]);

	std::string errStr;
	int samplingRate = atoi(argv[2]);
	int numChannels = atoi(argv[3]);
	bool floatSamples = (atoi(argv[4]) == 0)?false:true;
	double extraDelay = strtod(argv[5], nullptr);

	MIPTime interval(0.020); // We'll use 20 milliseconds
	MIPAlsaInput sndCardInput;
	MIPAudioMixer mixer;
	MIPAlsaOutput sndCardOutput;
	MyChain chain("Sound file player");
	bool returnValue;

	returnValue = sndCardInput.open(samplingRate, numChannels, interval, floatSamples, devName);
	checkError(returnValue, sndCardInput);

	returnValue = mixer.init(samplingRate, numChannels, interval, false, floatSamples);
	checkError(returnValue, mixer);

	returnValue = mixer.setExtraDelay(extraDelay);
	checkError(returnValue, mixer);

	returnValue = sndCardOutput.open(samplingRate, numChannels, interval, floatSamples, devName);
	checkError(returnValue, sndCardOutput);

	// Next, we'll create the chain
	returnValue = chain.setChainStart(&sndCardInput);
	checkError(returnValue, chain);

	//MIPMessageDumper msgDmp;
	//chain.addConnection(&sndCardInput, &msgDmp);

	returnValue = chain.addConnection(&sndCardInput, &mixer);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&mixer, &sndCardOutput);
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

