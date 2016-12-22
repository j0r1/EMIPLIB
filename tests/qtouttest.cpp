#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_QT5) && defined(MIPCONFIG_SUPPORT_AVCODEC) && defined(MIPCONFIG_SUPPORT_VIDEO4LINUX)

#include "mipv4l2input.h"
#include "mipqt5outputwidget.h"
#include "mipcomponentchain.h"
#include "mipaveragetimer.h"
#include "miptinyjpegdecoder.h"
#include "mipcomponentalias.h"
#include "mipencodedvideomessage.h"
#include "miprawvideomessage.h"
#include "mipavcodecframeconverter.h"
#include "mipoutputmessagequeuesimple.h"
#include <QtGui/QApplication>
#include <QtCore/QThread>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMdiArea>
#include <iostream>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif // WIN32

using namespace std;

void checkError(bool returnValue, const MIPComponent &component)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in component: " << component.getComponentName() << std::endl;
	std::cerr << "Error description: " << component.getErrorString() << std::endl;

	_exit(-1); // a regular exit causes a segfault when using a QOpenGLWindow
}

void checkError(bool returnValue, const MIPComponentChain &chain)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in chain: " << chain.getName() << std::endl;
	std::cerr << "Error description: " << chain.getErrorString() << std::endl;

	_exit(-1); // a regular exit causes a segfault when using a QOpenGLWindow
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

// This component will change the source ID regularly, to see what happens
// when sources are added and timed out
class ChangeSourceComponent : public MIPOutputMessageQueueSimple
{
public:
	ChangeSourceComponent() : MIPOutputMessageQueueSimple("ChangeSourceComponent")
	{
		m_prevTime = MIPTime::getCurrentTime();
		m_sourceID = 1;
	}

	~ChangeSourceComponent()
	{
	}
protected:
	MIPMessage *pushSub(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg, 
			            bool &deleteMessage, bool &error)
	{
		if (pMsg->getMessageType() != MIPMESSAGE_TYPE_VIDEO_RAW)
		{
			error = true;
			setErrorString("Bad message");
			return nullptr;
		}

		MIPTime now = MIPTime::getCurrentTime();
		MIPTime elapsed = now;
		
		elapsed -= m_prevTime;

		//cout << "elapsed.getValue() = " << elapsed.getValue() << endl;
		if (elapsed.getValue() > 15.0)
		{
			m_prevTime = now;
			m_sourceID++;
		}

		// We're not going to create a copy, we're just going to change the source ID
		MIPVideoMessage *pVidMsg = static_cast<MIPVideoMessage *>(pMsg);
		pVidMsg->setSourceID(m_sourceID);
		deleteMessage = false;
		return pVidMsg;
	}
private:
	MIPTime m_prevTime;
	uint64_t m_sourceID;
};


int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	
	MIPTime interval(0.050); // We'll use 50 millisecond intervals
	MIPAverageTimer timer(interval);
	MIPV4L2Input videoIn;
	MIPComponentAlias videoInAlias(&videoIn);
	MIPTinyJPEGDecoder jpegDec;
	MIPQt5OutputComponent outputComp;
	MIPAVCodecFrameConverter frameConv;
	ChangeSourceComponent changeSource;
	MyChain chain("Qt video test");

	uint32_t tgtSubType = 0;
	while (!tgtSubType)
	{
		cout << R"XYZ(
Please select a raw video message type to test:

 1) YUV420P
 2) RGB32
 3) RGB24
 4) YUYV

)XYZ" << endl;
		int choice = 0;
		cin >> choice;

		switch(choice)
		{
		case 1: 
			tgtSubType = MIPRAWVIDEOMESSAGE_TYPE_YUV420P;
			break;
		case 2:
			tgtSubType = MIPRAWVIDEOMESSAGE_TYPE_RGB32;
			break;
		case 3:
			tgtSubType = MIPRAWVIDEOMESSAGE_TYPE_RGB24;
			break;
		case 4:
			tgtSubType = MIPRAWVIDEOMESSAGE_TYPE_YUYV;
			break;
		}
	}

	checkError(videoIn.open(640, 480), videoIn);
	checkError(jpegDec.init(), jpegDec);
	checkError(frameConv.init(-1, -1, tgtSubType), frameConv);
	checkError(outputComp.init(), outputComp);

	checkError(chain.setChainStart(&timer), chain);
	checkError(chain.addConnection(&timer, &videoIn), chain);

	checkError(chain.addConnection(&videoIn, &jpegDec, false,
			   MIPMESSAGE_TYPE_VIDEO_ENCODED, MIPENCODEDVIDEOMESSAGE_TYPE_JPEG), chain);
	checkError(chain.addConnection(&videoIn, &videoInAlias, false, 0, 0), chain);
	
	uint32_t allowedSubtypes = MIPRAWVIDEOMESSAGE_TYPE_YUV420P | MIPRAWVIDEOMESSAGE_TYPE_RGB24 | MIPRAWVIDEOMESSAGE_TYPE_RGB32 | MIPRAWVIDEOMESSAGE_TYPE_YUYV;
	checkError(chain.addConnection(&jpegDec, &frameConv, false, MIPMESSAGE_TYPE_VIDEO_RAW, allowedSubtypes), chain);
	checkError(chain.addConnection(&videoInAlias, &frameConv, false, MIPMESSAGE_TYPE_VIDEO_RAW, allowedSubtypes), chain);

	checkError(chain.addConnection(&frameConv, &changeSource), chain);
	checkError(chain.addConnection(&changeSource, &outputComp), chain);

	checkError(chain.start(), chain);
	int status = 0;
	
	// This block makes sure that 'observer' is removed before the pointer to
	// outputComp becomes invalid
	{
		MIPQt5OutputMDIWidget mainWin(&outputComp);

		mainWin.show();
		status = app.exec();
	}

	chain.stop();
	return status;
}

#endif // MIPCONFIG_SUPPORT_QT5 && MIPCONFIG_SUPPORT_AVCODEC && MIPCONFIG_SUPPORT_VIDEO4LINUX
