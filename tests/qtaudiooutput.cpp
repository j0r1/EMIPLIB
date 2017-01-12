#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_QT5

#include "mipcomponentchain.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipaveragetimer.h"
#include "mipwavinput.h"
#include "mipsampleencoder.h"
#include "mipqt5audiooutput.h"
#include <QWidget>
#include <QApplication>
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstdlib>
#ifndef WIN32
#include <unistd.h>
#endif // WIN32

using namespace std;

void checkError(bool returnValue, const MIPComponent &component)
{
	if (returnValue == true)
		return;

	cerr << "An error occured in component: " << component.getComponentName() << endl;
	cerr << "Error description: " << component.getErrorString() << endl;

	_exit(-1);
}

void checkError(bool returnValue, const MIPComponentChain &chain)
{
	if (returnValue == true)
		return;

	cerr << "An error occured in chain: " << chain.getName() << endl;
	cerr << "Error description: " << chain.getErrorString() << endl;

	_exit(-1);
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

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	MIPTime interval(0.020);
	MIPTime interval2(0.020);
	MIPAverageTimer timer(interval2);
	MIPWAVInput sndFileInput;
	MIPSampleEncoder sampEnc;
	MIPQt5AudioOutput sndCardOutput;
	MyChain chain("Sound file player");
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
	returnValue = sndCardOutput.open(samplingRate, numChannels, interval);
	checkError(returnValue, sndCardOutput);

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

	QWidget w;
	w.show();

	app.exec();

	chain.stop();

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

