/**
 * \file simplechain.cpp
 */

#include <mipcomponentchain.h>
#include <mipcomponent.h>
#include <miptime.h>
#include <mipaveragetimer.h>
#include <mipmessagedumper.h>
#include <iostream>
#include <string>
#include <unistd.h>

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
	// We'll initialize the timer to generate messages after 0.5 seconds.
	MIPAverageTimer timer(MIPTime(0.5));
	MIPMessageDumper msgDump;
	MyChain chain("Simple chain");
	bool returnValue;

	// The timing component and message dumper don't require further
	// initialization, so we'll just add them to the chain.

	returnValue = chain.setChainStart(&timer);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&timer, &msgDump);
	checkError(returnValue, chain);

	// Now, we can start the chain.

	returnValue = chain.start();
	checkError(returnValue, chain);

	// We'll wait five seconds before stopping the chain.
	MIPTime::wait(MIPTime(5.0));

	returnValue = chain.stop();
	checkError(returnValue, chain);

	return 0;
}

