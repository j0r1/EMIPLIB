/**
 * \file feedbackexample.cpp
 */

#include <mipconfig.h>

#if(defined(MIPCONFIG_SUPPORT_OSS) || defined(MIPCONFIG_SUPPORT_WINMM) || defined(MIPCONFIG_SUPPORT_PORTAUDIO))

#include <mipcomponentchain.h>
#include <mipcomponent.h>
#include <miptime.h>
#include <mipaveragetimer.h>
#include <mipwavinput.h>
#include <mipsamplingrateconverter.h>
#include <mipsampleencoder.h>
#include <mipulawencoder.h>
#include <miprtpulawencoder.h>
#include <miprtpcomponent.h>
#include <miprtpdecoder.h>
#include <miprtpulawdecoder.h>
#include <mipulawdecoder.h>
#include <mipaudiomixer.h>
#include <miprawaudiomessage.h> // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE etc
#ifdef MIPCONFIG_SUPPORT_WINMM
	#include <mipwinmmoutput.h>
#else
#ifdef MIPCONFIG_SUPPORT_OSS
	#include <mipossinputoutput.h>
#else
	#include <mippainputoutput.h>
	#define NEED_PA_INIT
#endif
#endif 
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtperrors.h>
#include <stdio.h>
#include <iostream>
#include <string>

using namespace jrtplib;

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

// We'll be using an RTPSession instance from the JRTPLIB library. The following
// function checks the JRTPLIB error code.

void checkError(int status)
{
	if (status >= 0)
		return;
	
	std::cerr << "An error occured in the RTP component: " << std::endl;
	std::cerr << "Error description: " << RTPGetErrorString(status) << std::endl;
	
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
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // WIN32

	MIPTime interval(0.020); // We'll use 20 millisecond intervals.
	MIPAverageTimer timer(interval);
	MIPWAVInput sndFileInput;
	MIPSamplingRateConverter sampConv, sampConv2;
	MIPSampleEncoder sampEnc, sampEnc2, sampEnc3;
	MIPULawEncoder uLawEnc;
	MIPRTPULawEncoder rtpEnc;
	MIPRTPComponent rtpComp;
	MIPRTPDecoder rtpDec;
	MIPRTPULawDecoder rtpULawDec;
	MIPULawDecoder uLawDec;
	MIPAudioMixer mixer;
#ifdef MIPCONFIG_SUPPORT_WINMM
	MIPWinMMOutput sndCardOutput;
#else
#ifdef MIPCONFIG_SUPPORT_OSS
	MIPOSSInputOutput sndCardOutput;
#else
	MIPPAInputOutput sndCardOutput;
#endif
#endif
	MyChain chain("Sound file player");
	RTPSession rtpSession;
	bool returnValue;

	// We'll open the file 'soundfile.wav'.

	returnValue = sndFileInput.open("soundfile.wav", interval);
	checkError(returnValue, sndFileInput);
	
	// We'll convert to a sampling rate of 8000Hz and mono sound.
	
	int samplingRate = 8000;
	int numChannels = 1;

	returnValue = sampConv.init(samplingRate, numChannels);
	checkError(returnValue, sampConv);

	// Initialize the sample encoder: the RTP U-law audio encoder
	// expects native endian signed 16 bit samples.
	
	returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16);
	checkError(returnValue, sampEnc);

	// Convert samples to U-law encoding
	returnValue = uLawEnc.init();
	checkError(returnValue, uLawEnc);

	// Initialize the RTP audio encoder: this component will create
	// RTP messages which can be sent to the RTP component.

	returnValue = rtpEnc.init();
	checkError(returnValue, rtpEnc);

	// We'll initialize the RTPSession object which is needed by the
	// RTP component.
	
	RTPUDPv4TransmissionParams transmissionParams;
	RTPSessionParams sessionParams;
	int portBase = 60000;
	int status;

	transmissionParams.SetPortbase(portBase);
	sessionParams.SetOwnTimestampUnit(1.0/((double)samplingRate));
	sessionParams.SetMaximumPacketSize(64000);
	sessionParams.SetAcceptOwnPackets(true);
	
	status = rtpSession.Create(sessionParams,&transmissionParams);
	checkError(status);

	// Instruct the RTP session to send data to ourselves.
	status = rtpSession.AddDestination(RTPIPv4Address(ntohl(inet_addr("127.0.0.1")),portBase));
	checkError(status);

	// Tell the RTP component to use this RTPSession object.
	returnValue = rtpComp.init(&rtpSession);
	checkError(returnValue, rtpComp);
	
	// Initialize the RTP audio decoder.
	returnValue = rtpDec.init(true, 0, &rtpSession);
	checkError(returnValue, rtpDec);

	// Register the U-law decoder for payload type 0
	returnValue = rtpDec.setPacketDecoder(0,&rtpULawDec);
	checkError(returnValue, rtpDec);

	// Convert U-law encoded samples to linear encoded samples
	returnValue = uLawDec.init();
	checkError(returnValue, uLawDec);

	// Transform the received audio data to floating point format.
	returnValue = sampEnc2.init(MIPRAWAUDIOMESSAGE_TYPE_FLOAT);
	checkError(returnValue, sampEnc2);

	// We'll make sure that received audio frames are converted to the right
	// sampling rate.
	returnValue = sampConv2.init(samplingRate, numChannels);
	checkError(returnValue, sampConv2);

	// Initialize the mixer.
	returnValue = mixer.init(samplingRate, numChannels, interval);
	checkError(returnValue, mixer);

	// Initialize the soundcard output.
	returnValue = sndCardOutput.open(samplingRate, numChannels, interval);
	checkError(returnValue, sndCardOutput);

#ifdef MIPCONFIG_SUPPORT_WINMM
	// The WinMM output component uses signed little endian 16 bit samples.
	returnValue = sampEnc3.init(MIPRAWAUDIOMESSAGE_TYPE_S16LE);
#else
#ifdef MIPCONFIG_SUPPORT_OSS
	// The OSS component can use several encoding types. We'll ask
	// the component to which format samples should be converted.
	returnValue = sampEnc3.init(sndCardOutput.getRawAudioSubtype());
#else
	// The PortAudio output component uses signed 16 bit samples
	returnValue = sampEnc3.init(MIPRAWAUDIOMESSAGE_TYPE_S16);
#endif
#endif
	checkError(returnValue, sampEnc3);

	// Next, we'll create the chain
	returnValue = chain.setChainStart(&timer);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&timer, &sndFileInput);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sndFileInput, &sampConv);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sampConv, &sampEnc);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sampEnc, &uLawEnc);
	checkError(returnValue, chain);
	
	returnValue = chain.addConnection(&uLawEnc, &rtpEnc);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&rtpEnc, &rtpComp);
	checkError(returnValue, chain);
	
	returnValue = chain.addConnection(&rtpComp, &rtpDec);
	checkError(returnValue, chain);

	// This is where the feedback chain is specified: we want
	// feedback from the mixer to reach the RTP audio decoder,
	// so we'll specify that over the links in between, feedback
	// should be transferred.

	returnValue = chain.addConnection(&rtpDec, &uLawDec, true);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&uLawDec, &sampEnc2, true);
	checkError(returnValue, chain);
	
	returnValue = chain.addConnection(&sampEnc2, &sampConv2, true);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sampConv2, &mixer, true);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&mixer, &sampEnc3);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&sampEnc3, &sndCardOutput);
	checkError(returnValue, chain);
	
	// Start the chain

	returnValue = chain.start();
	checkError(returnValue, chain);

	// We'll wait until enter is pressed

	int num = 10;

	for (int i = 0 ; i < num ; i++)
	{
		std::cout << "iteration " << (i+1) << "/" << num << std::endl;
		std::cout << "Press enter for silence" << std::endl;

		getc(stdin);
		rtpComp.lock();
		rtpComp.setEnableSending(false);
		rtpComp.unlock();

		std::cout << "Press enter for sound" << std::endl;

		getc(stdin);
		rtpComp.lock();
		rtpComp.setEnableSending(true);
		rtpComp.unlock();
	}

	returnValue = chain.stop();
	checkError(returnValue, chain);

	rtpSession.Destroy();
	
	// We'll let the destructors of the other components take care
	// of their de-initialization.

	sndCardOutput.close(); // In case we're using PortAudio
#ifdef NEED_PA_INIT
	MIPPAInputOutput::terminatePortAudio();
#endif // NEED_PA_INIT

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

#endif // WIN32 || _WIN32_WCE || MIPCONFIG_SUPPORT_OSS

