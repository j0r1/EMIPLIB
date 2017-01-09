#include "mipconfig.h"
#include "mipcomponentchain.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipaveragetimer.h"
#include "mipwavinput.h"
#include "mipopusencoder.h"
#include "miprtpopusencoder.h"
#include "miprtpcomponent.h"
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpipv4address.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstdlib>

using namespace std;
using namespace jrtplib;

void checkError(bool returnValue, const MIPComponent &component)
{
	if (returnValue == true)
		return;

	cerr << "An error occured in component: " << component.getComponentName() << endl;
	cerr << "Error description: " << component.getErrorString() << endl;

	exit(-1); 
}

void checkError(bool returnValue, const MIPComponentChain &chain)
{
	if (returnValue == true)
		return;

	cerr << "An error occured in chain: " << chain.getName() << endl;
	cerr << "Error description: " << chain.getErrorString() << endl;

	exit(-1);
}

void checkError(int err)
{
	if (err >= 0)
		return;

	cerr << "Error " << err << endl;
	cerr << RTPGetErrorString(err) << endl;

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

class MyRTPSession : public RTPSession
{
public:
	MyRTPSession(int repeats = 4) : m_repeats(repeats)
	{
		SetChangeOutgoingData(true);
	}
private:
	int OnChangeRTPOrRTCPData(const void *origdata, size_t origlen, bool isrtp, void **senddata, size_t *sendlen)
	{
		*senddata = (void*)origdata;
		*sendlen = origlen;
		if (isrtp)
		{
			for (int i = 0 ; i < m_repeats-1 ; i++)
				RTPSession::SendRawData(origdata, origlen, true);
		}
	}
	
	int m_repeats;
};

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		cerr << "Specify wav file and repeats" << endl;
		return -1;
	}

	MyChain chain("Audio codec test");
	MIPTime interval(0.020);
	MIPWAVInput sndFileInput;
	MIPOpusEncoder opusEnc;
	MIPRTPOpusEncoder opusRTPEnc;
	MIPRTPComponent rtpComp;
	MyRTPSession rtpSession(atoi(argv[2]));

	checkError(sndFileInput.open(argv[1], interval), sndFileInput);

	int rate = sndFileInput.getSamplingRate();
	int channels = sndFileInput.getNumberOfChannels();
	checkError(opusEnc.init(rate, channels, MIPOpusEncoder::Audio, interval), opusEnc);
	
	checkError(opusRTPEnc.init(), opusRTPEnc);
	opusRTPEnc.setPayloadType(96);

	RTPUDPv4TransmissionParams transParams;
	RTPSessionParams params;

	transParams.SetPortbase(0); // should choose automatically
	params.SetOwnTimestampUnit(1.0/48000.0);
	checkError(rtpSession.Create(params, &transParams));
	checkError(rtpComp.init(&rtpSession), rtpComp);

	MIPAverageTimer timer(interval);

	checkError(chain.setChainStart(&timer), chain);
	checkError(chain.addConnection(&timer, &sndFileInput), chain);
	checkError(chain.addConnection(&sndFileInput, &opusEnc), chain);
	checkError(chain.addConnection(&opusEnc, &opusRTPEnc), chain);
	checkError(chain.addConnection(&opusRTPEnc, &rtpComp), chain);

	checkError(chain.start(), chain);

	//uint8_t ip[] = { 10,11,12,123 };
	uint8_t ip[] = { 224,2,36,42 };

	RTPIPv4Address dest(ip, 8002);
	checkError(rtpSession.AddDestination(dest));

	cout << "Type something to exit" << endl;
	string dummy;
	cin >> dummy;

	checkError(chain.stop(), chain);

	return 0;
}

