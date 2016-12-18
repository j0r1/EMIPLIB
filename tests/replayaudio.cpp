#include "mipconfig.h"
#include "mipavcodecencoder.h"
#include "mipaudiosession.h"
#include "mippainputoutput.h"
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpexternaltransmitter.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpbyteaddress.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <atomic>
#include <thread>

using namespace jrtplib;
using namespace std;

void checkRet(bool ret,const MIPErrorBase &obj)
{
	if (!ret)
	{
		std::cerr << obj.getErrorString() << std::endl;
		exit(-1);
	}
}

void checkError(int status)
{
	if (status >= 0)
		return;
	
	cerr << "An error occured in the RTP component: " << endl;
	cerr << "Error description: " << RTPGetErrorString(status) << endl;
	
	exit(-1);
}

class MyRTPSession : public RTPSession
{
protected:
	void OnPollThreadError(int err)
	{
		cerr << "POLL THREAD ERROR!" << endl;
		checkError(err);
	}
};

class DummySender : public RTPExternalSender
{
public:
	DummySender() { }
	bool SendRTP(const void *data, size_t len) { return true; }
	bool SendRTCP(const void *data, size_t len) { return true; }
	bool ComesFromThisSender(const RTPAddress *a) { return false; }
};

class MyAudioSession : public MIPAudioSession
{
protected:
	void onInputThreadExit(bool err, const std::string &compName, const std::string &errStr)
	{
		if (err)
		{
			std::cerr << "Input chain thread exited due to an error" << std::endl;
			std::cerr << "Component: " << compName << std::endl;
			std::cerr << "Error: " << errStr << std::endl;
		}
	}
	void onOutputThreadExit(bool err, const std::string &compName, const std::string &errStr)
	{
		if (err)
		{
			std::cerr << "Output chain thread exited due to an error" << std::endl;
			std::cerr << "Component: " << compName << std::endl;
			std::cerr << "Error: " << errStr << std::endl;
		}
	}	
};

int main(void)
{
#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	string errStr;

	if (!MIPPAInputOutput::initializePortAudio(errStr))
	{
		std::cerr << "Can't initialize PortAudio: " << errStr << std::endl;
		return -1;
	}
#endif // MIPCONFIG_SUPPORT_PORTAUDIO
	
	MIPAudioSessionParams Aparams;
	MyAudioSession audioSess;
	bool ret;
	
	Aparams.setSpeexIncomingPayloadType(97);
	Aparams.setOpusIncomingPayloadType(98);

	FILE *pFile = fopen("logfile.dat", "rb");
	if (!pFile)
	{
		cerr << "Unable to open logfile.dat" << endl;
		return -1;
	}

	DummySender sender;
	RTPExternalTransmitter trans(0);
	RTPExternalTransmissionParams params(&sender, 20);

	checkError(trans.Init(true));
	checkError(trans.Create(65535, &params));

	RTPExternalTransmissionInfo *pTransInfo = (RTPExternalTransmissionInfo *)trans.GetTransmissionInfo();
	RTPExternalPacketInjecter *pPacketInjecter = pTransInfo->GetPacketInjector();
	trans.DeleteTransmissionInfo(pTransInfo);

	RTPSessionParams sessParams;

	sessParams.SetOwnTimestampUnit(1.0/5.0);
	sessParams.SetProbationType(RTPSources::NoProbation);

	MyRTPSession rtpSession;
	checkError(rtpSession.Create(sessParams, &trans));

	ret = audioSess.init(&Aparams, nullptr, &rtpSession);
	checkRet(ret, audioSess);

	atomic_bool done;
	
	done = false;
	thread t([&]{

		vector<uint8_t> data;
		double fileStartTime = 0;
		double realStartTime = 0;
		uint8_t host[] = {1, 2, 3, 4};
		RTPByteAddress addr(host, 4);

		while (!done)
		{
			char ident[4];
			double t;
			uint32_t dataLength;

			if (fread(ident, 1, 4, pFile) != 4 || fread(&t, 1, sizeof(double), pFile) != sizeof(double) ||
				fread(&dataLength, 1, sizeof(uint32_t), pFile) != sizeof(uint32_t))
				break;

			data.resize(dataLength);
			if (fread(&data[0], 1, dataLength, pFile) != dataLength)
				break;

			if (realStartTime == 0)
			{
				realStartTime = RTPTime::CurrentTime().GetDouble();
				fileStartTime = t;
			}
			else
			{
				// Busy wait until time to inject
				while (true)
				{
					double curTime = RTPTime::CurrentTime().GetDouble();
					if (curTime - realStartTime >= t - fileStartTime)
						break;
				}
			}

	//		if (ident[2] == 'C')
	//			cerr << "Injecting RTCP packet of length " << dataLength << endl;
			pPacketInjecter->InjectRTPorRTCP(&data[0], dataLength, addr);

			MIPTime::wait(MIPTime(0.001));
		}
	});

	string dummy;
	cout << "Type something to exit: " << endl;
	cin >> dummy;

	done = true;

	t.join();
	fclose(pFile);

	audioSess.destroy();

#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	MIPPAInputOutput::terminatePortAudio();
#endif // MIPCONFIG_SUPPORT_PORTAUDIO
	
	return 0;
}
