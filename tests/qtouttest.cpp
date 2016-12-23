#include "mipconfig.h"

#include <iostream>

using namespace std;

#if defined(MIPCONFIG_SUPPORT_QT5) && defined(MIPCONFIG_SUPPORT_AVCODEC)

#include "mipqt5output.h"
#include "mipcomponentchain.h"
#include "mipaveragetimer.h"
#include "miptinyjpegdecoder.h"
#include "mipcomponentalias.h"
#include "mipencodedvideomessage.h"
#include "miprawvideomessage.h"
#include "mipavcodecframeconverter.h"
#include "mipoutputmessagequeuesimple.h"
#include "mipyuv420fileinput.h"
#include <QtWidgets/QApplication>
#include <QtCore/QThread>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMdiArea>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif // WIN32

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

	if (argc != 4)
	{
		cerr << "Usage: " << argv[0] << " file.yuv width height" << endl;
		return -1;
	}
	
	MIPTime interval(0.040); // We'll use 50 millisecond intervals
	MIPAverageTimer timer(interval);
	MIPYUV420FileInput videoIn;
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

	checkError(videoIn.open(argv[1],atoi(argv[2]), atoi(argv[3])), videoIn);
	checkError(frameConv.init(-1, -1, tgtSubType), frameConv);
	checkError(outputComp.init(), outputComp);

	checkError(chain.setChainStart(&timer), chain);
	checkError(chain.addConnection(&timer, &videoIn), chain);
	checkError(chain.addConnection(&videoIn, &frameConv), chain);
	checkError(chain.addConnection(&frameConv, &changeSource), chain);
	checkError(chain.addConnection(&changeSource, &outputComp), chain);

	int status = 0;
	
	// This block makes sure that the window is removed before the pointer to
	// outputComp becomes invalid
	{
		MIPQt5OutputMDIWidget mainWin(&outputComp);

		// Actually start the chain. Now that we've initialized the window, we
		// can be sure that all Qt signals (about new source for example) will
		// be received
		checkError(chain.start(), chain);

		mainWin.show();
		status = app.exec();
	}

	chain.stop();
	return status;
}

#else

int main(void)
{
	cerr << "Not all necessary components are available to run this example." << endl;
	return -1;
}

#endif // MIPCONFIG_SUPPORT_QT5 && MIPCONFIG_SUPPORT_AVCODEC 
