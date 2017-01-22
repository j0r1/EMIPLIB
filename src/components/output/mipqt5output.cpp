/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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

#include "mipqt5output.h"

#ifdef MIPCONFIG_SUPPORT_QT5

#include "mipmessage.h"
#include "miprawvideomessage.h"
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLBuffer>
#include <QtCore/QTimer>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QMdiSubWindow>
#include <QtCore/QDebug>
#include <jthread/jmutexautolock.h>
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>

#define MIPQT5OUTPUTCOMPONENT_ERRSTR_ALREADYINIT			"Qt5 output component is already initialized"
#define MIPQT5OUTPUTCOMPONENT_ERRSTR_CANTINITMUTEX			"Can't initialize mutex: Error code "
#define MIPQT5OUTPUTCOMPONENT_ERRSTR_NEGATIVETIMEOUT		"A positive timeout must be specified"
#define MIPQT5OUTPUTCOMPONENT_ERRSTR_NOTINIT				"The Qt5 output component is not initialized yet"
#define MIPQT5OUTPUTCOMPONENT_ERRSTR_BADMESSAGE				"Can't interpret message, must be a raw video message of YUV420, YUYV or RGB subtype"
#define MIPQT5OUTPUTCOMPONENT_ERRSTR_NOPULL					"The Qt5 output component does not implement the pull method"

using namespace std;
using namespace jthread;

static const char s_vertexShader[] = R"XYZ( 
precision mediump float;

attribute vec2 a_position;
attribute vec2 a_texcoord;
varying vec2 v_pos;
varying vec2 v_tpos;

void main()
{
    v_pos = vec2(a_position.x, a_position.y);
	v_tpos = vec2(a_texcoord.x, a_texcoord.y);
    gl_Position = vec4(a_position.x, a_position.y, 0, 1);
}
)XYZ";

static const char s_fragmentShaderYUYV[] = R"XYZ( 
precision mediump float;

uniform sampler2D u_tex;
uniform float u_width;
varying vec2 v_pos;
varying vec2 v_tpos;

void main()
{
	float xInt = floor(u_width*v_tpos.x);
	float xIntEven = floor(xInt/2.0)*2.0;
	float evenFlag = xInt-xIntEven;
	vec4 cYUYV = texture2D(u_tex, vec2(v_tpos.x, v_tpos.y));

	float Y = cYUYV.x*(1.0-evenFlag) + cYUYV.z*evenFlag;
	float U = cYUYV.y;
	float V = cYUYV.a;

	float r = (1.164*(Y*255.0-16.0) + 1.596*(V-0.5)*255.0)/255.0;
	float g = (1.164*(Y*255.0-16.0) - 0.813*(V-0.5)*255.0 - 0.391*(U-0.5)*255.0)/255.0;
	float b = (1.164*(Y*255.0-16.0) + 2.018*(U-0.5)*255.0)/255.0;

	gl_FragColor = vec4(r, g, b, 1.0);
}
)XYZ";

static const char s_fragmentShaderRGB[] = R"XYZ( 
precision mediump float;

uniform sampler2D u_tex;
varying vec2 v_pos;
varying vec2 v_tpos;

void main()
{
	gl_FragColor = texture2D(u_tex, v_tpos);
	//gl_FragColor = vec4(1.0,0.0,0.0,1.0);
}
)XYZ";

static const char s_fragmentShaderYUV420[] = R"XYZ( 
precision mediump float;

uniform sampler2D u_Ytex;
uniform sampler2D u_UVtex;
varying vec2 v_pos;
varying vec2 v_tpos;

void main()
{
	vec4 cy = texture2D(u_Ytex, v_tpos);
	vec2 uPos = vec2(v_tpos.x, v_tpos.y/2.0);
	vec2 vPos = vec2(v_tpos.x, 0.5+v_tpos.y/2.0);
	vec4 cu = texture2D(u_UVtex, uPos);
	vec4 cv = texture2D(u_UVtex, vPos);

	float Y = cy.x;
	float U = cu.x;
	float V = cv.x;

	float r = (1.164*(Y*255.0-16.0) + 1.596*(V-0.5)*255.0)/255.0;
	float g = (1.164*(Y*255.0-16.0) - 0.813*(V-0.5)*255.0 - 0.391*(U-0.5)*255.0)/255.0;
	float b = (1.164*(Y*255.0-16.0) + 2.018*(U-0.5)*255.0)/255.0;

	gl_FragColor = vec4(r, g, b, 1.0);
}
)XYZ";

#define OUTPUTCLASSNAME MIPQt5OutputWindow
#define PARENTCLASSNAME QOpenGLWindow
#define REQUESTUPDATE requestUpdate()
#include "mipqt5outputtemplate.h"
#undef OUTPUTCLASSNAME
#undef PARENTCLASSNAME
#undef REQUESTUPDATE

#define OUTPUTCLASSNAME MIPQt5OutputWidget
#define PARENTCLASSNAME QOpenGLWidget
#define REQUESTUPDATE update()
#include "mipqt5outputtemplate.h"
#undef OUTPUTCLASSNAME
#undef PARENTCLASSNAME
#undef REQUESTUPDATE

// Component

MIPQt5OutputComponent::MIPQt5OutputComponent() : MIPComponent("MIPQt5OutputComponent")
{
	m_init = false;
}

MIPQt5OutputComponent::~MIPQt5OutputComponent()
{
	destroy();
}

bool MIPQt5OutputComponent::init(MIPTime sourceTimeout)
{
	if (m_init)
	{
		setErrorString(MIPQT5OUTPUTCOMPONENT_ERRSTR_ALREADYINIT);
		return false;
	}

	if (!m_mutex.IsInitialized())
	{
		int status;
	
		if ((status = m_mutex.Init()) < 0)
		{
			ostringstream ss;
			
			ss << MIPQT5OUTPUTCOMPONENT_ERRSTR_CANTINITMUTEX << status;
			setErrorString(ss.str());
			return false;
		}
	}

	MIPTime nul(0);
	if (sourceTimeout <= nul)
	{
		setErrorString(MIPQT5OUTPUTCOMPONENT_ERRSTR_NEGATIVETIMEOUT);
		return false;
	}

	m_sourceTimeout = sourceTimeout;
	m_init = true;
	return true;
}

bool MIPQt5OutputComponent::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPQT5OUTPUTCOMPONENT_ERRSTR_NOTINIT);
		return false;
	}

	// Make sure that when the widget is destroyed, this component is no longer contacted
	m_mutex.Lock();
	for (auto pWindow : m_windows)
		pWindow->clearComponent();
	for (auto pWindow : m_widgets)
		pWindow->clearComponent();
	m_mutex.Unlock();

	return true;
}

bool MIPQt5OutputComponent::getCurrentlyKnownSourceIDs(std::list<uint64_t> &sourceIDs)
{
	if (!m_init)
	{
		setErrorString(MIPQT5OUTPUTCOMPONENT_ERRSTR_NOTINIT);
		return false;
	}

	m_mutex.Lock();
	sourceIDs.clear();
	for (auto &keyVal : m_sourceTimes)
		sourceIDs.push_back(keyVal.first);
	m_mutex.Unlock();

	return true;
}

MIPQt5OutputWindow *MIPQt5OutputComponent::createWindow(uint64_t sourceID)
{
	MIPQt5OutputWindow *pWindow = new MIPQt5OutputWindow(this, sourceID); // This registers itself with this instance
	return pWindow;
}

MIPQt5OutputWidget *MIPQt5OutputComponent::createWidget(uint64_t sourceID)
{
	MIPQt5OutputWidget *pWindow = new MIPQt5OutputWidget(this, sourceID); // This registers itself with this instance
	return pWindow;
}

bool MIPQt5OutputComponent::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	assert(pMsg);

	uint32_t subType = pMsg->getMessageSubtype();
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && 
		  (subType == MIPRAWVIDEOMESSAGE_TYPE_YUV420P || subType == MIPRAWVIDEOMESSAGE_TYPE_RGB32  ||
	       subType == MIPRAWVIDEOMESSAGE_TYPE_RGB24 || subType == MIPRAWVIDEOMESSAGE_TYPE_YUYV) ) )
	{
		setErrorString(MIPQT5OUTPUTCOMPONENT_ERRSTR_BADMESSAGE);
		return false;
	}

	// Find the window and just send a copy of the message, let the window handle it
	MIPVideoMessage *pVidMsg = static_cast<MIPVideoMessage *>(pMsg);
	uint64_t sourceID = pVidMsg->getSourceID();

	m_mutex.Lock();
	for (auto it = m_windows.begin() ; it != m_windows.end() ; it++)
	{
		auto *pWin = *it;
		if (pWin->getSourceID() == sourceID)
		{
			MIPVideoMessage *pCopy = static_cast<MIPVideoMessage *>(pVidMsg->createCopy());
			pWin->injectFrame(pCopy);
		}
	}
	for (auto it = m_widgets.begin() ; it != m_widgets.end() ; it++)
	{
		auto *pWin = *it;
		if (pWin->getSourceID() == sourceID)
		{
			MIPVideoMessage *pCopy = static_cast<MIPVideoMessage *>(pVidMsg->createCopy());
			pWin->injectFrame(pCopy);
		}
	}
	m_mutex.Unlock();

	// Check for a new source
	auto it = m_sourceTimes.find(sourceID);
	MIPTime now = MIPTime::getCurrentTime();

	if (it == m_sourceTimes.end())
	{
		m_sourceTimes[sourceID] = now;
		emit signalNewSource(sourceID);
	}
	else
		it->second = now;

	double timeout = m_sourceTimeout.getValue(); // remove stream if inactive for a certain period

	vector<uint64_t> timedOutSources;

	m_mutex.Lock();
	it = m_sourceTimes.begin();
	while (it != m_sourceTimes.end())
	{
		if (now.getValue() - it->second.getValue() > timeout)
		{
			uint64_t sourceID = it->first;
			auto it2 = it;
			++it;
			m_sourceTimes.erase(it2);
			timedOutSources.push_back(sourceID);
	
			// Remove sourceID from m_windows
			{
				auto iw = m_windows.begin();
				while (iw != m_windows.end())
				{
					auto *pWin = *iw;
					if (pWin->getSourceID() == sourceID)
					{
						auto iw2 = iw;
						++iw;

						m_windows.erase(iw2);
						pWin->clearComponent(); // make sure the window no longer contacts this component
					}
					else
						++iw;
				}
			}
			{
				auto iw = m_widgets.begin();
				while (iw != m_widgets.end())
				{
					auto *pWin = *iw;
					if (pWin->getSourceID() == sourceID)
					{
						auto iw2 = iw;
						++iw;

						m_widgets.erase(iw2);
						pWin->clearComponent(); // make sure the window no longer contacts this component
					}
					else
						++iw;
				}
			}
		}
		else
			++it;
	}
	m_mutex.Unlock();
	
	for (uint64_t sourceID : timedOutSources)
	{
		emit signalRemovedSource(sourceID);
	}
	return true;
}

bool MIPQt5OutputComponent::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPQT5OUTPUTCOMPONENT_ERRSTR_NOPULL);
	return false;
}

void MIPQt5OutputComponent::registerWindow(MIPQt5OutputWindow *pWindow)
{
	assert(pWindow);
	JMutexAutoLock l(m_mutex);

	m_windows.insert(pWindow);
}

void MIPQt5OutputComponent::unregisterWindow(MIPQt5OutputWindow *pWindow)
{
	assert(pWindow);
	JMutexAutoLock l(m_mutex);

	auto it = m_windows.find(pWindow);
	if (it == m_windows.end())
	{
		cerr << "Warning: MIPQt5OutputComponent::unregisterWindow: couldn't find widget" << endl;
		return;
	}

	m_windows.erase(it);
}

void MIPQt5OutputComponent::registerWindow(MIPQt5OutputWidget *pWindow)
{
	assert(pWindow);
	JMutexAutoLock l(m_mutex);

	m_widgets.insert(pWindow);
}

void MIPQt5OutputComponent::unregisterWindow(MIPQt5OutputWidget *pWindow)
{
	assert(pWindow);
	JMutexAutoLock l(m_mutex);

	auto it = m_widgets.find(pWindow);
	if (it == m_widgets.end())
	{
		cerr << "Warning: MIPQt5OutputComponent::unregisterWindow: couldn't find widget" << endl;
		return;
	}

	m_widgets.erase(it);
}

// MDI test widget

#define USEWINDOW

MIPQt5OutputMDIWidget::MIPQt5OutputMDIWidget(MIPQt5OutputComponent *pComp) : m_pComponent(pComp)
{
	if (pComp)
	{
		QObject::connect(pComp, &MIPQt5OutputComponent::signalNewSource, this, &MIPQt5OutputMDIWidget::slotNewSource);
		QObject::connect(pComp, &MIPQt5OutputComponent::signalRemovedSource, this, &MIPQt5OutputMDIWidget::slotRemovedSource);
	}

	m_pMdiArea = new QMdiArea(this);
	setCentralWidget(m_pMdiArea);
}

MIPQt5OutputMDIWidget::~MIPQt5OutputMDIWidget()
{
}

void MIPQt5OutputMDIWidget::slotResize(int w, int h)
{
#ifdef USEWINDOW
	MIPQt5OutputWindow *pWin = qobject_cast<MIPQt5OutputWindow*>(sender());
	if (!pWin)
	{
		cerr << "Warning: got resize signal, but couldn't get window" << endl;
		return;
	}
#else
	MIPQt5OutputWidget *pWin = qobject_cast<MIPQt5OutputWidget*>(sender());
	if (!pWin)
	{
		cerr << "Warning: got resize signal, but couldn't get window" << endl;
		return;
	}
#endif
	//cout << "Resizing " << pWin->getSourceID() << " to " << w << "x" << h << endl;
	QString sourceStr = QString::number(pWin->getSourceID());

	for (auto &pSubWin : m_pMdiArea->subWindowList())
	{
		if (pSubWin->windowTitle() == sourceStr)
		{
			pSubWin->resize(w, h);
			//cout << "Found sub window" << endl;
		}
	}
}

void MIPQt5OutputMDIWidget::slotNewSource(quint64 sourceID)
{
	QString sourceStr = QString::number(sourceID);

#ifdef USEWINDOW
	MIPQt5OutputWindow *pWin = m_pComponent->createWindow(sourceID);

	QWidget *pWidget = QWidget::createWindowContainer(pWin, m_pMdiArea);
#else
	MIPQt5OutputWidget *pWidget = m_pComponent->createWidget(sourceID);
#endif
	QMdiSubWindow *pMdiWin = new QMdiSubWindow(m_pMdiArea);
	pMdiWin->setWidget(pWidget);
	pMdiWin->setWindowTitle(sourceStr);
	pMdiWin->setAttribute(Qt::WA_DeleteOnClose);
	pMdiWin->show();

	pWidget->show();
	pWidget->resize(100,100);

#ifdef USEWINDOW
	QObject::connect(pWin, &MIPQt5OutputWindow::signalResize, this, &MIPQt5OutputMDIWidget::slotResize);
#else
	QObject::connect(pWidget, &MIPQt5OutputWidget::signalResize, this, &MIPQt5OutputMDIWidget::slotResize);
#endif
}

void MIPQt5OutputMDIWidget::slotRemovedSource(quint64 sourceID)
{
	QString sourceStr = QString::number(sourceID);

	for (auto &pSubWin : m_pMdiArea->subWindowList())
	{
		if (pSubWin->windowTitle() == sourceStr)
			pSubWin->close();
	}
}

#endif // MIPCONFIG_SUPPORT_QT5

