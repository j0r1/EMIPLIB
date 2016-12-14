/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2011  Hasselt University - Expertise Centre for
                      Digital Media (EDM) (http://www.edm.uhasselt.be)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  
  USA

*/

#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_QT)

#include "mipqtoutput.h"
#include "miptime.h"
#include "miprawvideomessage.h"
#include "mipcompat.h"
#include <qobject.h>
#include <qapplication.h>
#include <qframe.h>
#include <qmainwindow.h>
#include <qvbox.h>
#include <qworkspace.h>
#include <qevent.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <iostream>

#include "mipdebug.h"

#define MIPQTOUTPUT_ERRSTR_NOTRUNNING				"Output thread is not running"
#define MIPQTOUTPUT_ERRSTR_ALREADYRUNNING			"Output thread is already running"
#define MIPQTOUTPUT_ERRSTR_CANTSTARTTHREAD			"Couldn't start the background thread"
#define MIPQTOUTPUT_ERRSTR_BADMESSAGE				"Bad message"
#define MIPQTOUTPUT_ERRSTR_PULLNOTSUPPORTED			"Pull not supported"

#define YUVTORGB(Y,U,V,R,G,B) { \
				float r,g,b; \
						\
				r = 1.164*((float)(Y) - 16.0) + 1.596*((float)(V) - 128.0); \
				g = 1.164*((float)(Y) - 16.0) - 0.813*((float)(V) - 128.0) - 0.391*((float)(U) - 128.0); \
				b = 1.164*((float)(Y) - 16.0) + 2.018*((float)(U) - 128.0); \
							\
				r = (r > 255.0)?255.0:r; \
				g = (g > 255.0)?255.0:g; \
				b = (b > 255.0)?255.0:b; \
				r = (r < 0.0)?0.0:r; \
				g = (g < 0.0)?0.0:g; \
				b = (b < 0.0)?0.0:b; \
						\
				R = (uint8_t)r; \
				G = (uint8_t)g; \
				B = (uint8_t)b; \
			      }


class MIPQtOutputWidget : public QFrame
{
	Q_OBJECT
public:
	MIPQtOutputWidget(QWidget *parent,uint64_t id) : QFrame(parent)
	{
		m_sourceID = id;
		int status;
	
		if ((status = m_frameMutex.Init()) < 0)
		{
			std::cerr << "Error: can't initialize component mutex (JMutex error code " << status << ")" << std::endl;
			exit(-1);
		}	
		resize(100,100);
	
		QTimer *t = new QTimer(this);
		QObject::connect(t,SIGNAL(timeout()),this,SLOT(update()));
		t->start(40);
	
		m_width = -1;
		m_height = -1;
		//m_resize = false;
		m_pImgData = 0;
		m_converted = false;

		char str[256];

		MIP_SNPRINTF(str, 256, "%LX",id);

		setCaption(str);

		setWFlags(Qt::WRepaintNoErase);
	}

	~MIPQtOutputWidget()
	{
		if (m_pImgData)
			delete [] m_pImgData;
	}

	void paintEvent(QPaintEvent *e)
	{
		m_frameMutex.Lock();
		if (m_width > 0)
			resize(m_width,m_height);
		if (m_pImgData && !m_converted)
		{
			m_converted = true;
			if (m_tmpImg.isNull())
			{
				if ((m_width&1) == 0 && (m_height&1) == 0)
				{
					const uint8_t *pValues = m_pImgData;
					size_t totalNum = m_width*m_height;
					const uint8_t *pY = pValues;
					const uint8_t *pU = pValues+totalNum;
					const uint8_t *pV = pValues+totalNum+totalNum/4;
					
					int indexY = 0;
					int indexU = 0;
					int indexV = 0;
					
					for (int y = 0 ; y < m_height ; y += 2)
					{
						for (int x = 0 ; x < m_width ; x += 2)
						{
							uint8_t Y0 = pY[indexY + 0];
							uint8_t Y1 = pY[indexY + 1];
							uint8_t Y2 = pY[indexY + m_width];
							uint8_t Y3 = pY[indexY + m_width + 1];
							uint8_t U = pU[indexU];
							uint8_t V = pV[indexV];
							
							uint8_t R,G,B;
							
							YUVTORGB(Y0,U,V,R,G,B);
							m_img.setPixel(x, y, qRgb(R, G, B));
							YUVTORGB(Y1,U,V,R,G,B);
							m_img.setPixel(x + 1, y, qRgb(R, G, B));
							YUVTORGB(Y2,U,V,R,G,B);
							m_img.setPixel(x, y + 1, qRgb(R, G, B));
							YUVTORGB(Y3,U,V,R,G,B);
							m_img.setPixel(x + 1, y + 1, qRgb(R, G, B));
							
							indexY += 2;
							indexU++;
							indexV++;
							
						}
						indexY += m_width;
					}
				}
				else
				{
					for (int y = 0 ; y < m_height ; y++)
					{
						for (int x = 0 ; x < m_width ; x++)
						{
							m_img.setPixel(x,y,qRgb(0,0,0));
						}
					}
				}
			}
			else
				m_img = m_tmpImg.swapRGB();
		}
		m_frameMutex.Unlock();

		QPainter p;

		p.begin(this);
		p.drawImage(0,0,m_img);
		p.end();
	}

	void processRawVideoMessage(MIPVideoMessage *pVidMsg)
	{
		m_frameMutex.Lock();
		if (m_width < 0) // first time
		{
			m_width = pVidMsg->getWidth();
			m_height = pVidMsg->getHeight();

			if (pVidMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
			{
				m_img.create(m_width,m_height,32);
				m_pImgData = new uint8_t[m_width*m_height*3/2];
			}
			else // 32 bit rgb
			{
				m_pImgData = new uint8_t[m_width*m_height*4];
				m_tmpImg = QImage((uchar *)m_pImgData, m_width, m_height, 32, 0, 0, QImage::LittleEndian);
			}
			//m_resize = true;
		}
		else
		{
			//m_resize = false;
		}

		if (pVidMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
		{
			MIPRawYUV420PVideoMessage *pMsg = (MIPRawYUV420PVideoMessage *)pVidMsg;

			memcpy(m_pImgData,pMsg->getImageData(),m_width*m_height*3/2);
			m_converted = false;
		}
		else // 32 bit rgb
		{
			MIPRawRGBVideoMessage *pMsg = (MIPRawRGBVideoMessage *)pVidMsg;

			memcpy(m_pImgData,pMsg->getImageData(),m_width*m_height*4);
			m_converted = false;
		}
		m_frameMutex.Unlock();
	}
	
	uint64_t getSourceID() const
	{
		return m_sourceID;
	}
private:
	uint64_t m_sourceID;
	jthread::JMutex m_frameMutex;
	int m_width;
	int m_height;
	QImage m_img, m_tmpImg;
	//bool m_resize;
	uint8_t *m_pImgData;
	bool m_converted;
};

class MIPQtOutputMainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MIPQtOutputMainWindow(MIPQtOutput &qtOutput) : m_qtOutput(qtOutput)
	{
		QVBox *pBox = new QVBox(this);
		setCentralWidget(pBox);
		//resize(1080,940);
		
		m_pWorkspace = new QWorkspace(pBox);
		m_pWorkspace->setScrollBarsEnabled(true);
		m_pWorkspace->show();
			
		QTimer *t = new QTimer(this);
		
		QObject::connect(t,SIGNAL(timeout()),this,SLOT(checkNewIDs()));
		t->start(100);
	}
public slots:
	void checkNewIDs()
	{
		bool shouldClose = false;
		
		m_qtOutput.m_lock.Lock();

		std::list<uint64_t>::const_iterator it;

		for (it =  m_qtOutput.m_newIDs.begin() ; it != m_qtOutput.m_newIDs.end() ; it++)
		{
			MIPQtOutputWidget *w = new MIPQtOutputWidget(m_pWorkspace,*it);
			m_qtOutput.m_outputWidgets.push_back(w);
			w->show();
		}
		m_qtOutput.m_newIDs.clear();

		if (m_qtOutput.m_stopWindow)
			shouldClose = true;
			
		m_qtOutput.m_lock.Unlock();

		if (shouldClose)
		{
			emit stopSignal();
		}
	}
signals:
	void stopSignal();
private:
	MIPQtOutput &m_qtOutput;
	QWorkspace *m_pWorkspace;
};

MIPQtOutput::MIPQtOutput() : MIPComponent("MIPQtOutput")
{
	int status;
	
	if ((status = m_lock.Init()) < 0)
	{
		std::cerr << "Error: can't initialize Qt output mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPQtOutput::~MIPQtOutput()
{
	shutdown();
}

bool MIPQtOutput::init()
{
	if (JThread::IsRunning())
	{
		setErrorString(MIPQTOUTPUT_ERRSTR_ALREADYRUNNING);
		return false;
	}

	m_outputWidgets.clear();
	m_stopWindow = false;
	if (JThread::Start() < 0)
	{
		setErrorString(MIPQTOUTPUT_ERRSTR_CANTSTARTTHREAD);
		return false;
	}
	
	return true;
}

bool MIPQtOutput::shutdown()
{
	if (!JThread::IsRunning())
	{
		setErrorString(MIPQTOUTPUT_ERRSTR_NOTRUNNING);
		return false;
	}

	m_lock.Lock();
	m_stopWindow = true;
	m_lock.Unlock();

	//MIPTime::wait(MIPTime(1.500));

	MIPTime startTime = MIPTime::getCurrentTime();

	while (JThread::IsRunning() && (MIPTime::getCurrentTime().getValue() - startTime.getValue()) < 5.0)
		MIPTime::wait(MIPTime(0.050));
	
	if (JThread::IsRunning())
	{
		std::cerr << "Killing" << std::endl;
		JThread::Kill();
	}
	
	m_outputWidgets.clear();
	return true;
}

bool MIPQtOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!JThread::IsRunning())
	{
		setErrorString(MIPQTOUTPUT_ERRSTR_NOTRUNNING);
		return false;
	}
	
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && 
	     (pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUV420P || pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_RGB32))) 
	{
		setErrorString(MIPQTOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPVideoMessage *pVideoMsg = (MIPVideoMessage *)pMsg;
	uint64_t id = pVideoMsg->getSourceID();
	
	m_lock.Lock();
	
	bool found = false;
	std::list<MIPQtOutputWidget *>::const_iterator it;

	for (it = m_outputWidgets.begin() ; !found && it != m_outputWidgets.end() ; it++)
	{
		if ((*it)->getSourceID() == id)
		{
			found = true;
			(*it)->processRawVideoMessage(pVideoMsg);
		}
	}

	if (!found)
	{
		std::list<uint64_t>::const_iterator it;

		for (it = m_newIDs.begin() ; !found && it != m_newIDs.end() ; it++)
		{
			if ((*it) == id)
				found = true;
		}
		if (!found)
			m_newIDs.push_back(id);
	}
	
	m_lock.Unlock();
	
	return true;
}

bool MIPQtOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPQTOUTPUT_ERRSTR_PULLNOTSUPPORTED);
	return true;
}

void *MIPQtOutput::Thread()
{
#ifdef MIPDEBUG
	std::cout << "MIPQtOutput::Thread started" << std::endl;
#endif // MIPDEBUG

	int argc = 1;
	char *argv[] = { "MIPQtOutput", 0 };
	QApplication app(argc,argv);
	MIPQtOutputMainWindow mainWin(*this);
	
	app.setMainWidget(&mainWin);
	mainWin.show();
	QObject::connect(&mainWin,SIGNAL(stopSignal()),&app,SLOT(quit()));

	JThread::ThreadStarted();
	
	app.exec();

	bool done = false;
	
	do
	{
		MIPTime::wait(MIPTime(0.050));
		m_lock.Lock();
		done = m_stopWindow;
		m_lock.Unlock();
	} while (!done);

	//std::cerr << "Qt thread is exiting..." << std::endl;

#ifdef MIPDEBUG
	std::cout << "MIPQtOutput::Thread stopped" << std::endl;
#endif // MIPDEBUG

	return 0;
}

#include "mipqtoutput.moc"

#endif // MIPCONFIG_SUPPORT_QT

