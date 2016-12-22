/**
 * \file avsession.cpp
 */

#include <mipconfig.h>

#if defined(MIPCONFIG_SUPPORT_QT5) && defined(MIPCONFIG_SUPPORT_AVCODEC) \
	&& ( (defined(MIPCONFIG_SUPPORT_OSS) && defined(MIPCONFIG_SUPPORT_VIDEO4LINUX)) || \
	     (defined(MIPCONFIG_SUPPORT_WINMM) && defined(MIPCONFIG_SUPPORT_DIRECTSHOW)) )

#include <mipvideosession.h>
#include <mipavcodecencoder.h>
#include <mipaudiosession.h>
#include <mipqt5outputwidget.h>
#include <mippainputoutput.h>
#include <iostream>
#include <jrtplib3/rtpipv4address.h>
#include <QtGui/QApplication>
#include <QtWidgets/QMainWindow>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif // WIN32

using namespace jrtplib;

void checkRet(bool ret,const MIPErrorBase &obj)
{
	if (!ret)
	{
		std::cerr << obj.getErrorString() << std::endl;
		_exit(-1);
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

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
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
#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	std::string errStr;

	if (!MIPPAInputOutput::initializePortAudio(errStr))
	{
		std::cerr << "Can't initialize PortAudio: " << errStr << std::endl;
		return -1;
	}
#endif // MIPCONFIG_SUPPORT_PORTAUDIO
	
	MIPAVCodecEncoder::initAVCodec();
	
	MIPVideoSessionParams Vparams;
	MIPAudioSessionParams Aparams;
	MyVideoSession videoSess;
	MyAudioSession audioSess;
	bool ret;

	std::string bandWidth;
	std::cout << "Enter bandwidth for video (in bits per second, e.g. 200000)" << std::endl;
	std::cin >> bandWidth;

	int bw = atoi(bandWidth.c_str());
	int audioPort = 6000;
	int videoPort = 6100;
	
	Vparams.setPortbase(videoPort);
	Vparams.setAcceptOwnPackets(true);
//	Vparams.setDevice("/dev/video0");
	Vparams.setFrameRate(25.0);
	Vparams.setBandwidth(bw);
	Vparams.setWidth(320);
	Vparams.setHeight(240);
//	Vparams.setUseQtOutput(false);
//	Vparams.setEncodingType(MIPVideoSessionParams::IntYUV420);
//	Vparams.setSessionType(MIPVideoSessionParams::OutputOnly);
	
	Aparams.setPortbase(audioPort);
//	Aparams.setCompressionType(MIPAudioSessionParams::Speex);
//	Aparams.setAcceptOwnPackets(true);
//	Aparams.setInputDevice("/dev/dsp1");
//	Aparams.setOutputDevice("/dev/dsp");
//	Aparams.setSpeexEncoding(MIPAudioSessionParams::UltraWideBand);
//	Aparams.setSpeexOutgoingPayloadType(97);
//	Aparams.setSpeexIncomingPayloadType(97);
	
	std::cout << "Starting audio session at portbase " << audioPort << ", video session at portbase " << videoPort << std::endl;

	std::string destStr;
	std::cout << "Enter destination IP, audio portbase and video portbase (format: a.b.c.d:AudioPort:VideoPort)" << std::endl;
	std::cout << "(Note that video is automatically sent to yourself in this example)" << std::endl;
	std::cin >> destStr;

	int ip[4] = { 127, 0, 0, 1};
	int Aport = 6000;
	int Vport = 6100;
	int status = sscanf(destStr.c_str(), "%d.%d.%d.%d:%d:%d", &ip[0], &ip[1], &ip[2], &ip[3], &Aport, &Vport);
	if (status != 6)
	{
		std::cerr << "Bad destination" << std::endl;
		return -1;
	}

	// We'll need to create the GUI very shortly after initializing the
	// session, to make sure that we're not missing the Qt signal that a
	// new source was added

	ret = videoSess.init(&Vparams);
	checkRet(ret, videoSess);

	ret = audioSess.init(&Aparams);
	checkRet(ret, audioSess);

	uint8_t ipLocal[4] = { 127, 0, 0, 1 };
	ret = videoSess.addDestination(RTPIPv4Address(ipLocal, videoPort));
//	ret = audioSess.addDestination(RTPIPv4Address(ipLocal, audioPort));

	{
		uint8_t ip2[4];

		ip2[0] = (uint8_t)ip[0];
		ip2[1] = (uint8_t)ip[1];
		ip2[2] = (uint8_t)ip[2];
		ip2[3] = (uint8_t)ip[3];
		ret = videoSess.addDestination(RTPIPv4Address(ip2,Vport));
		ret = audioSess.addDestination(RTPIPv4Address(ip2,Aport));
	
		MIPQt5OutputComponent *pOutputComp = videoSess.getQt5OutputComponent();
		if (pOutputComp == 0)
		{
			std::cerr << videoSess.getErrorString() << std::endl;
		}
		else
		{
			std::cout << "Starting test GUI, close main window to exit..." << std::endl;
			MIPQt5OutputMDIWidget testWidget(pOutputComp);

			testWidget.show();
			app.exec();
		}
	}
	
	videoSess.destroy();
	audioSess.destroy();

#ifdef MIPCONFIG_SUPPORT_PORTAUDIO
	MIPPAInputOutput::terminatePortAudio();
#endif // MIPCONFIG_SUPPORT_PORTAUDIO
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
	std::cerr << "Not all necessary components are available to run this example." << std::endl;
	return 0;
}

#endif 

