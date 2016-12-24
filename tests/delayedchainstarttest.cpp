#include "mipconfig.h"
#include "mipcomponentchain.h"
#include "mipaveragetimer.h"
#include "mippainputoutput.h"
#include "mipwinmminput.h"
#include "mipossinputoutput.h"
#include "mipjackinput.h"
#include <iostream>
#include <atomic>
#include <memory>
#include <vector>

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
		m_exited = false;
	}

	bool start()
	{
		m_exited = false;
		return MIPComponentChain::start();
	}

	bool exited() const
	{
		return m_exited;
	}
private:
	void onThreadExit(bool error, const std::string &errorComponent, const std::string &errorDescription)
	{
		m_exited = true;

		if (!error)
			return;

		std::cerr << "  An error occured in the background thread." << std::endl;
		std::cerr << "    Component: " << errorComponent << std::endl;
		std::cerr << "    Error description: " << errorDescription << std::endl;
	}	

	atomic_bool m_exited;
};

class TimingAnalyzer : public MIPComponent
{
public:
	TimingAnalyzer(int stopAfterInterval) : MIPComponent("TimingAnalyzer"), m_stopCount(stopAfterInterval) { }
	~TimingAnalyzer() { }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
	{
		fprintf(stderr, "\r  Iteration = %d                                   \r", (int)iteration);
		fflush(stderr);
		if (iteration == (int64_t)m_stopCount)
		{
			setErrorString("Stopping after requested number of intervals was reached");
			return false;
		}
		return true;
	}

	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
	{
		setErrorString("Pull not supported");
		return false;
	}
private:
	int m_stopCount;
};

MIPComponent *getAverageTimer(MIPTime interval)
{
	return new MIPAverageTimer(interval);
}

MIPComponent *getPAInputOutput(MIPTime interval)
{
#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	std::string errStr;

	if (!MIPPAInputOutput::initializePortAudio(errStr))
	{
		std::cerr << "Can't initialize PortAudio: " << errStr << std::endl;
		exit(-1);
	}

	MIPPAInputOutput *pComp = new MIPPAInputOutput();
	checkError(pComp->open(44100, 1, interval, MIPPAInputOutput::ReadWrite), *pComp);
	return pComp;
#else
	cerr << "No PortAudio support" << endl;
	return nullptr;
#endif 
}

MIPComponent *getWinMMInput(MIPTime interval)
{
#ifdef MIPCONFIG_SUPPORT_WINMM
	MIPWinMMInput *pComp = new MIPWinMMInput();
	checkError(pComp->open(44100, 1, interval), *pComp);
	return pComp;
#else
	cerr << "No WinMM support" << endl;
	return nullptr;
#endif
}

MIPComponent *getOSSInputOutput(MIPTime interval)
{
#ifdef MIPCONFIG_SUPPORT_OSS
	MIPOSSInputOutput *pComp = new MIPOSSInputOutput();
	checkError(pComp->open(44100, 1, interval, MIPOSSInputOutput::ReadWrite), *pComp);
	return pComp;
#else
	cerr << "No OSS support" << endl;
	return nullptr;
#endif
}

MIPComponent *getJACKInput(MIPTime interval)
{
#ifdef MIPCONFIG_SUPPORT_JACK
	MIPJackInput *pComp = new MIPJackInput();
	checkError(pComp->open(interval), *pComp);
	return pComp;
#else
	cerr << "No JACK support" << endl;
	return nullptr;
#endif
}

int main(void)
{
	MyChain chain("Delayed chain start test");
	MIPTime interval(0.020);
	vector<double> delays { 0.0, 5.0 };
	int chainIntervals = 150; // 150*0.020 is 3 seconds

	MIPComponent * (*getStartCompFunction)(MIPTime interval) = nullptr;

	while (!getStartCompFunction)
	{
		cout << R"XYZ(
Specify the chain start component to test:
 
 1) MIPAverageTimer
 2) MIPPAInputOutput
 3) MIPWinMMInput
 4) MIPOSSInputOutput
 5) MIPJackInput
)XYZ" << endl;

		int choice = 0;
		cin >> choice;

		if (choice == 1)
			getStartCompFunction = getAverageTimer;
		else if (choice == 2)
			getStartCompFunction = getPAInputOutput;
		else if (choice == 3)
			getStartCompFunction = getWinMMInput;
		else if (choice == 4)
			getStartCompFunction = getOSSInputOutput;
		else if (choice == 5)
			getStartCompFunction = getJACKInput;
		else
			cerr << "Invalid choice!" << endl;
	}
	
	for (double delay : delays)
	{
		MIPComponent *pStartComp = getStartCompFunction(interval);
		if (pStartComp == 0)
		{
			cerr << "No component could be obtained" << endl;
			return -1;
		}

		cout << endl;
		cout << "Testing component: " << pStartComp->getComponentName() << endl;
		cout << "Testing delay: " << delay << endl;

		TimingAnalyzer timingAnalyzer(chainIntervals);

		checkError(chain.setChainStart(pStartComp), chain);
		checkError(chain.addConnection(pStartComp, &timingAnalyzer), chain);

		MIPTime delayBeforeStart(delay);
		cout << "  Waiting " << delayBeforeStart.getString() << " seconds before starting chain" << endl;
		MIPTime::wait(delayBeforeStart);

		MIPTime startTime = MIPTime::getCurrentTime();

		checkError(chain.start(), chain);
		cout << "  Waiting for chain to stop" << endl;
		while (!chain.exited())
			MIPTime::wait(MIPTime(0.001)); // avoid a tight loop

		MIPTime endTime = MIPTime::getCurrentTime();

		double dt = endTime.getValue() - startTime.getValue();
		dt /= chainIntervals;

		cout << "  Requested interval is: " << interval.getValue() << endl;
		cout << "  Obtained interval is:  " << dt << endl;  
		cout << endl;

		delete pStartComp;
	}
	return 0;
}
