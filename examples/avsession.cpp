/**
 * \file avsession.cpp
 */

#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_QT) && defined(MIPCONFIG_SUPPORT_AVCODEC) && defined(MIPCONFIG_SUPPORT_SPEEX)

#include "mipvideosession.h"
#include "mipavcodecencoder.h"
#include "mipaudiosession.h"
#include <iostream>
#include <rtpipv4address.h>

void checkRet(bool ret,const MIPErrorBase &obj)
{
	if (!ret)
	{
		std::cerr << obj.getErrorString() << std::endl;
		exit(-1);
	}
}

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

class MyVideoSession : public MIPVideoSession
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
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);

	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		WSACleanup();
		std::cout << "Couldn't initialize COM" << std::endl;
		return -1;
	}
#endif // WIN32
	
	MIPAVCodecEncoder::initAVCodec();
	
	MIPVideoSessionParams Vparams;
	MIPAudioSessionParams Aparams;
	MyVideoSession videoSess;
	MyAudioSession audioSess;
	bool ret;

	std::string bandWidth;
	std::cout << "Enter bandwidth" << std::endl;
	std::cin >> bandWidth;

	int bw = atoi(bandWidth.c_str());
	
	Vparams.setPortbase(6100);
	Vparams.setAcceptOwnPackets(true);
	Vparams.setDevice("/dev/video0");
	Vparams.setFrameRate(25.0);
	Vparams.setBandwidth(bw);
	
	Aparams.setPortbase(6000);
//	Aparams.setAcceptOwnPackets(true);
//	Aparams.setInputDevice("/dev/dsp1");
//	Aparams.setOutputDevice("/dev/dsp");
	
	std::cout << "Starting audio session at portbase 6000, video session at portbase 6100" << std::endl;

	ret = videoSess.init(&Vparams);
	checkRet(ret, videoSess);

	ret = audioSess.init(&Aparams);
	checkRet(ret, audioSess);

	uint8_t ipLocal[4] = { 127, 0, 0, 1 };
	ret = videoSess.addDestination(RTPIPv4Address(ipLocal,6100));
//	ret = audioSess.addDestination(RTPIPv4Address(ipLocal,6000));

	int ip[4] = { 127, 0, 0, 1};
	int Aport = 6000;
	int Vport = 6100;
	
	std::string destStr;
	std::cout << "Enter destination IP, audio portbase and video portbase (format: a.b.c.d:AudioPort:VideoPort)" << std::endl;
	std::cin >> destStr;

	int status = sscanf(destStr.c_str(), "%d.%d.%d.%d:%d:%d", &ip[0], &ip[1], &ip[2], &ip[3], &Aport, &Vport);
	if (status != 6)
	{
		std::cerr << "Bad destination" << std::endl;
	}
	else
	{
		uint8_t ip2[4];

		ip2[0] = (uint8_t)ip[0];
		ip2[1] = (uint8_t)ip[1];
		ip2[2] = (uint8_t)ip[2];
		ip2[3] = (uint8_t)ip[3];
		ret = videoSess.addDestination(RTPIPv4Address(ip2,Vport));
		ret = audioSess.addDestination(RTPIPv4Address(ip2,Aport));
	
		std::string str;
		std::cout << "Type something to exit" << std::endl;
		std::cin >> str;
	}
	
	videoSess.destroy();
	audioSess.destroy();

#ifdef WIN32
	CoUninitialize();	
	WSACleanup();
#endif // WIN32
	
	return 0;
}

#else

#include <iostream>

int main(void)
{
	std::cerr << "This application needs Qt support, libavcodec support and Speex support." << std::endl;
	return 0;
}

#endif // MIPCONFIG_SUPPORT_SPEEX && MIPCONFIG_SUPPORT_QT && MIPCONFIG_SUPPORT_AVCODEC

