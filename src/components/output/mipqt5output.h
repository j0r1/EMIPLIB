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

/**
 * \file mipqt5output.h
 */

#ifndef MIPQT5OUTPUT_H

#define MIPQT5OUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_QT5

#include "mipcomponent.h"
#include "miptime.h"
#include <QtGui/QOpenGLWindow>
#include <QtWidgets/QOpenGLWidget>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLTexture>
#include <QtWidgets/QMainWindow>
#include <QtCore/QMutex>
#include <jthread/jmutex.h>
#include <map>
#include <set>
#include <vector>

class MIPQt5OutputComponent;
class MIPVideoMessage;
class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QMdiArea;

/** A `QWindow`-based output window that can display the video from a particular source.
 *
 *  This is a `QWindow`-based output window that can display the video from a particular 
 *  source. An instance is retrieved by calling MIPQt5OutputComponent::createWindow.
 *  The video data is shown using OpenGL.
 *
 *  Note that this is a `QWindow`-based window, not a `QWidget`-based one. To use it in a
 *  `QWidget`-based application you'll need to wrap the window in a widget using the
 *  `QWidget::createWindowContainer` function.
 */
class MIPQt5OutputWindow : public QOpenGLWindow, public QOpenGLFunctions
{
	Q_OBJECT
private:
	MIPQt5OutputWindow(MIPQt5OutputComponent *pComp, uint64_t sourceID);
public:
	~MIPQt5OutputWindow();

	/** Retrieves the source ID from which the video data is shown. */
	uint64_t getSourceID() const														{ return m_sourceID; }

	/** Retrieves the last known width of the video, or -1 if no frame was processed yet. */
	int getVideoWidth();

	/** Retrieves the last known height of the video, or -1 if no frame was processed yet. */
	int getVideoHeight();
private slots:
	void slotInternalNewFrame(MIPVideoMessage *pMsg);
signals:
	/** This Qt signal is triggered when the width of the video changes. */
	void signalResizeWidth(int w);

	/** This Qt signal is triggered when the height of the video changes. */
	void signalResizeHeight(int h);

	/** This Qt signal is triggered when the width, height or both of the video change. */
	void signalResize(int w, int h);

	void signalInternalNewFrame(MIPVideoMessage *pMsg);
private:
	void initializeGL();
	void paintGL();
	void resizeGL(int w, int h);
	void clearComponent();
	void injectFrame(MIPVideoMessage *pMsg);
	GLuint createTexture();
	void checkDisplayRoutines();

	bool m_init;
	MIPQt5OutputComponent *m_pComponent;
	QMutex m_mutex;
	uint64_t m_sourceID;

	MIPVideoMessage *m_pFrameToProcess;
	int m_prevWidth, m_prevHeight;

	QOpenGLShaderProgram *m_pYUV420Program;
	QOpenGLShaderProgram *m_pRGBProgram;
	QOpenGLShaderProgram *m_pYUYVProgram;
	QOpenGLShaderProgram *m_pLastUsedProgram;
	GLint m_tex0, m_tex1;
	GLint m_widthLoc;

	friend class MIPQt5OutputComponent;
};

/** TODO */
class MIPQt5OutputWidget : public QOpenGLWidget, public QOpenGLFunctions
{
	Q_OBJECT
private:
	MIPQt5OutputWidget(MIPQt5OutputComponent *pComp, uint64_t sourceID);
public:
	~MIPQt5OutputWidget();

	/** Retrieves the source ID from which the video data is shown. */
	uint64_t getSourceID() const														{ return m_sourceID; }

	/** Retrieves the last known width of the video, or -1 if no frame was processed yet. */
	int getVideoWidth();

	/** Retrieves the last known height of the video, or -1 if no frame was processed yet. */
	int getVideoHeight();
private slots:
	void slotInternalNewFrame(MIPVideoMessage *pMsg);
signals:
	/** This Qt signal is triggered when the width of the video changes. */
	void signalResizeWidth(int w);

	/** This Qt signal is triggered when the height of the video changes. */
	void signalResizeHeight(int h);

	/** This Qt signal is triggered when the width, height or both of the video change. */
	void signalResize(int w, int h);

	void signalInternalNewFrame(MIPVideoMessage *pMsg);
private:
	void initializeGL();
	void paintGL();
	void resizeGL(int w, int h);
	void clearComponent();
	void injectFrame(MIPVideoMessage *pMsg);
	GLuint createTexture();
	void checkDisplayRoutines();

	bool m_init;
	MIPQt5OutputComponent *m_pComponent;
	QMutex m_mutex;
	uint64_t m_sourceID;

	MIPVideoMessage *m_pFrameToProcess;
	int m_prevWidth, m_prevHeight;

	QOpenGLShaderProgram *m_pYUV420Program;
	QOpenGLShaderProgram *m_pRGBProgram;
	QOpenGLShaderProgram *m_pYUYVProgram;
	QOpenGLShaderProgram *m_pLastUsedProgram;
	GLint m_tex0, m_tex1;
	GLint m_widthLoc;

	friend class MIPQt5OutputComponent;
};

/** To easily visualise incoming video frames from multiple sources, this MDI widget
 *  can be used.
 *
 *  To easily visualise incoming video frames from multiple sources, this MDI widget
 *  can be used. The constructor takes a MIPQt5OutputComponent instance as a parameter,
 *  and will show the video data that this component receives.  
 */
class MIPQt5OutputMDIWidget : public QMainWindow
{
	Q_OBJECT
public:
	/** Specify the MIPQt5OutputComponent instance as the argument to this constructor
	 *  to indicate that the video streams received by that component should be shown. */
	MIPQt5OutputMDIWidget(MIPQt5OutputComponent *pComp);
	~MIPQt5OutputMDIWidget();
private slots:
	void slotResize(int w, int h);
	void slotNewSource(quint64 sourceID);
	void slotRemovedSource(quint64 sourceID);
private:
	MIPQt5OutputComponent *m_pComponent;
	QMdiArea *m_pMdiArea;
};

/** This component can be used to show video streams if Qt5 support is enabled.
 *
 *  This component can be used to show video streams if Qt5 support is enabled. When
 *  video data from a new source is detected, the Qt signal MIPQt5OutputComponent::signalNewSource
 *  is triggered. To create a window with the video data for this source, use
 *  the MIPQt5OutputComponent::createWindow function. When no data was received for
 *  a specific source for a certain time, the MIPQt5OutputComponent::signalRemovedSource
 *  signal is triggered.
 *
 *  For easy visualization you can pass this component to the constructor of
 *  MIPQt5OutputMDIWidget. This will connect to the signals mentioned above and
 *  create and remove windows with the video streams automatically.
 */
class MIPQt5OutputComponent : public QObject, public MIPComponent
{
	Q_OBJECT
public:
	MIPQt5OutputComponent();
	~MIPQt5OutputComponent();

	/** Initializes the component, specifying the timeout that should be used
	 *  to detect if a source has become inactive (at which time the signal
	 *  MIPQt5OutputComponent::signalRemovedSource will be triggered). */
	bool init(MIPTime sourceTimeout = MIPTime(20.0));

	/** De-initialized the component. */
	bool destroy();

	/** Creates a window for the specified source, in which the corresponding video 
	 *  data will be shown. 
	 *
	 *  Creates a window for the specified source, in which the corresponding video 
	 *  data will be shown. This is a `QWindow`-based window, not a `QWidget`-based one.
	 *  To use this in a `QWidget`-based application, use `QWidget::createWindowContainer`
	 *  to wrap the window in a widget. */
	MIPQt5OutputWindow *createWindow(uint64_t sourceID);

	/** TODO */
	MIPQt5OutputWidget *createWidget(uint64_t sourceID);

	/** Retrieves a list of the sources that are currently sending data, or that
	 *  have not yet been timed out. */
	bool getCurrentlyKnownSourceIDs(std::list<uint64_t> &sourceIDs);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
signals:
	/** This Qt signal is triggered when data from a new source has been detected. */
	void signalNewSource(quint64 sourceID);

	/** This Qt signal is triggered when a source was timed out. */
	void signalRemovedSource(quint64 sourceID);
private:
	void registerWindow(MIPQt5OutputWindow *pWin);
	void unregisterWindow(MIPQt5OutputWindow *pWin);
	void registerWindow(MIPQt5OutputWidget *pWin);
	void unregisterWindow(MIPQt5OutputWidget *pWin);

	bool m_init;
	std::map<uint64_t, MIPTime> m_sourceTimes;
	std::set<MIPQt5OutputWindow *> m_windows;
	std::set<MIPQt5OutputWidget *> m_widgets;
	jthread::JMutex m_mutex;
	MIPTime m_sourceTimeout;

	friend class MIPQt5OutputWindow;
	friend class MIPQt5OutputWidget;
};

#endif // MIPCONFIG_SUPPORT_QT5

#endif // MIPQT5OUTPUT_H
