/**
 * \file feedbackexample.cpp
 */

#include <mipcomponentchain.h>
#include <mipcomponent.h>
#include <miptime.h>
#include <mipaveragetimer.h>
#include <mipwavinput.h>
#include <mipsamplingrateconverter.h>
#include <mipsampleencoder.h>
#ifndef WIN32
	#include <mipossinputoutput.h>
#else
	#include <mipwinmmoutput.h>
#endif 
#include <mipaudiomixer.h>
#include <miprtpaudioencoder.h>
#include <miprtpcomponent.h>
#include <miprtpaudiodecoder.h>
#include <mipaudiomixer.h>
#include <miprawaudiomessage.h> // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE etc
#include <rtpsession.h>
#include <rtpsessionparams.h>
#include <rtpipv4address.h>
#include <rtpudpv4transmitter.h>
#include <rtperrors.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
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
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // WIN32

	MIPTime interval(0.020); // We'll use 20 millisecond intervals.
	MIPAverageTimer timer(interval);
	MIPWAVInput sndFileInput;
	MIPSamplingRateConverter sampConv, sampConv2;
	MIPSampleEncoder sampEnc, sampEnc2, sampEnc3;
	MIPRTPAudioEncoder rtpEnc;
	MIPRTPComponent rtpComp;
	MIPRTPAudioDecoder rtpDec;
	MIPAudioMixer mixer;
#ifndef WIN32
	MIPOSSInputOutput sndCardOutput;
#else
	MIPWinMMOutput sndCardOutput;
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

	// Initialize the sample encoder: the RTP audio encoder
	// expects big endian unsigned 16 bit samples.
	
	returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_U16BE);
	checkError(returnValue, sampEnc);

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
	
	// Initialize the RTP audio packet decoder.
	returnValue = rtpDec.init();
	checkError(returnValue, rtpDec);

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

#ifndef WIN32
	// The OSS component can use several encodings. We'll check
	// what encoding type is being used and inform the sample encoder
	// of this.
	uint32_t audioSubtype = sndCardOutput.getRawAudioSubtype();
	returnValue = sampEnc3.init(audioSubtype);
#else
	// The WinMM soundcard output component uses 16 bit signed little
	// endian data.
	returnValue = sampEnc3.init(MIPRAWAUDIOMESSAGE_TYPE_S16LE);
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

	returnValue = chain.addConnection(&sampEnc, &rtpEnc);
	checkError(returnValue, chain);

	returnValue = chain.addConnection(&rtpEnc, &rtpComp);
	checkError(returnValue, chain);
	
	returnValue = chain.addConnection(&rtpComp, &rtpDec);
	checkError(returnValue, chain);

	// This is where the feedback chain is specified: we want
	// feedback from the mixer to reach the RTP audio decoder,
	// so we'll specify that over the links in between, feedback
	// should be transferred.

	returnValue = chain.addConnection(&rtpDec, &sampEnc2, true);
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

	getc(stdin);

	returnValue = chain.stop();
	checkError(returnValue, chain);

	rtpSession.Destroy();
	
	// We'll let the destructors of the other components take care
	// of their de-initialization.

#ifdef WIN32
	WSACleanup();
#endif
	return 0;
}

