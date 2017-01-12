#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_QT5) && defined(MIPCONFIG_SUPPORT_OPUS)

#include "mipcomponentchain.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipaveragetimer.h"
#include "mipsampleencoder.h"
#include "miprtpopusdecoder.h"
#include "mipopusdecoder.h"
#include "miprtpcomponent.h"
#include "miprtpdecoder.h"
#include "mipaudiomixer.h"
#include "mipqt5audiooutput.h"
#include "mipmediabuffer.h"
#include "mipsamplingrateconverter.h"
#include <QApplication>
#include <QWidget>
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtperrors.h>
#include <stdio.h>
#include <iostream>
#include <string>
#ifndef WIN32
#include <unistd.h>
#endif // WIN32

using namespace jrtplib;

void checkError(bool returnValue, const MIPComponent &component)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in component: " << component.getComponentName() << std::endl;
	std::cerr << "Error description: " << component.getErrorString() << std::endl;

	_exit(-1);
}

void checkError(bool returnValue, const MIPComponentChain &chain)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in chain: " << chain.getName() << std::endl;
	std::cerr << "Error description: " << chain.getErrorString() << std::endl;

	_exit(-1);
}

// We'll be using an RTPSession instance from the JRTPLIB library. The following
// function checks the JRTPLIB error code.

void checkError(int status)
{
	if (status >= 0)
		return;
	
	std::cerr << "An error occured in the RTP component: " << std::endl;
	std::cerr << "Error description: " << RTPGetErrorString(status) << std::endl;
	
	_exit(-1);
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
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // WIN32

	QApplication app(argc, argv);

	MIPTime interval(0.020); // We'll use 20 millisecond intervals.
	MIPAverageTimer timer(interval);
	MIPRTPComponent rtpComp;
	MIPRTPDecoder rtpDec;
	MIPRTPOpusDecoder rtpOpusDec;
	MIPMediaBuffer mediaBuffer;
	MIPOpusDecoder opusDec;
	MIPSamplingRateConverter sampConv;
	MIPAudioMixer mixer;
	MIPQt5AudioOutput sndCardOutput;
	MyChain chain("Opus multicast receiver");
	RTPSession rtpSession;
	bool returnValue;

	int samplingRate = 48000;
	int numChannels = 1;

	// We'll initialize the RTPSession object which is needed by the
	// RTP component.
	
	RTPUDPv4TransmissionParams transmissionParams;
	RTPSessionParams sessionParams;
	int portBase = 8002;
	int status;

	transmissionParams.SetPortbase(portBase);
	sessionParams.SetOwnTimestampUnit(1.0/((double)samplingRate));
	sessionParams.SetMaximumPacketSize(64000);
	
	status = rtpSession.Create(sessionParams,&transmissionParams);
	checkError(status);

	uint8_t ip[4] = {224, 2, 36, 42 };
	status = rtpSession.JoinMulticastGroup(RTPIPv4Address(ip));
	checkError(status);

	// Tell the RTP component to use this RTPSession object.
	returnValue = rtpComp.init(&rtpSession);
	checkError(returnValue, rtpComp);
	
	// Initialize the RTP audio decoder.
	returnValue = rtpDec.init(true, nullptr, &rtpSession);
	checkError(returnValue, rtpDec);

	// Register the U-law decoder for payload type 96
	returnValue = rtpDec.setPacketDecoder(96,&rtpOpusDec);
	checkError(returnValue, rtpDec);

	returnValue = mediaBuffer.init(interval);
	checkError(returnValue, mediaBuffer);

	// Convert U-law encoded samples to linear encoded samples
	returnValue = opusDec.init(samplingRate, numChannels, false);
	checkError(returnValue, opusDec);

	// We'll make sure that received audio frames are converted to the right
	// sampling rate.
	returnValue = sampConv.init(samplingRate, numChannels, false);
	checkError(returnValue, sampConv);

	// Initialize the mixer.
	returnValue = mixer.init(samplingRate, numChannels, interval, true, false);
	checkError(returnValue, mixer);

	// Initialize the soundcard output.
	returnValue = sndCardOutput.open(samplingRate, numChannels, interval);
	checkError(returnValue, sndCardOutput);

	// Next, we'll create the chain
	returnValue = chain.setChainStart(&timer);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&timer, &rtpComp);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&rtpComp, &rtpDec);
	checkError(returnValue, chain);

	// This is where the feedback chain is specified: we want
	// feedback from the mixer to reach the RTP audio decoder,
	// so we'll specify that over the links in between, feedback
	// should be transferred.

	returnValue = chain.addConnection(&rtpDec, &mediaBuffer, true);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&mediaBuffer, &opusDec, true);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&opusDec, &sampConv, true);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sampConv, &mixer, true);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&mixer, &sndCardOutput);
	checkError(returnValue, chain);

	// Start the chain

	returnValue = chain.start();
	checkError(returnValue, chain);

	QWidget w;
	w.show();
	app.exec();

	chain.stop();

	rtpSession.Destroy();
	
	// We'll let the destructors of the other components take care
	// of their de-initialization.

#ifdef WIN32
	WSACleanup();
#endif
	return 0;
}

#else

#include <iostream>

int main(void)
{
	std::cerr << "Not all necessary components are available to run this example." << std::endl;
	return 0;
}

#endif 

