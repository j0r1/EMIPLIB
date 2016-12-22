#include "mipqt5outputwidget.h"

#ifdef MIPCONFIG_SUPPORT_QT5

#include "mipmessage.h"
#include "miprawvideomessage.h"
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLBuffer>
#include <QtCore/QTimer>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QMdiSubWindow>
#include <jthread/jmutexautolock.h>
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>

using namespace std;
using namespace jthread;

#if 0 // debug code
#include <dlfcn.h>
#include <GL/glx.h>

static GLXContext (*test_glXGetCurrentContext)(void) = nullptr;
static void *test_library = nullptr;

static void initGLFunctions()
{
	if (test_library)
		return;

	test_library = dlopen("libGL.so", RTLD_LAZY);
	test_glXGetCurrentContext = (decltype(test_glXGetCurrentContext))dlsym(test_library, "glXGetCurrentContext");
	cerr << "test_glXGetCurrentContext = " << (void*)test_glXGetCurrentContext << endl;
}
#endif 

static const char s_vertexShader[] = R"XYZ( 
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
uniform sampler2D u_tex;
uniform float u_width;
varying vec2 v_pos;
varying vec2 v_tpos;

void main()
{
	vec4 cY = texture2D(u_tex, v_tpos);
	float xInt = floor(u_width*v_tpos.x);
	float xIntEven = floor(xInt/2.0)*2.0;

	vec4 cU = texture2D(u_tex, vec2((xIntEven+0.5)/u_width, v_tpos.y));
	vec4 cV = texture2D(u_tex, vec2((xIntEven+1.5)/u_width, v_tpos.y));

	float Y = cY.x;
	float U = cU.y;
	float V = cV.y;

	float r = (1.164*(Y*255.0-16.0) + 1.596*(V-0.5)*255.0)/255.0;
	float g = (1.164*(Y*255.0-16.0) - 0.813*(V-0.5)*255.0 - 0.391*(U-0.5)*255.0)/255.0;
	float b = (1.164*(Y*255.0-16.0) + 2.018*(U-0.5)*255.0)/255.0;

	gl_FragColor = vec4(r, g, b, 1.0);
	//gl_FragColor = vec4(cV.y, cV.y, cV.y, 1.0);
}
)XYZ";

static const char s_fragmentShaderRGB[] = R"XYZ( 
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

string loadShader(const string &fileName)
{
	ifstream f;
	stringstream ss;
	f.open(fileName);

	ss << f.rdbuf();
	return ss.str();
}

MIPQt5OutputWindow::MIPQt5OutputWindow(MIPQt5OutputComponent *pComp, uint64_t sourceID) 
	: QOpenGLWindow(), 
	m_init(false),
	m_pComponent(pComp), 
	m_sourceID(sourceID),
	m_pYUV420Program(nullptr),
	m_pRGBProgram(nullptr),
	m_pYUYVProgram(nullptr),
	m_pLastUsedProgram(nullptr),
	m_pFrameToProcess(nullptr),
	m_prevWidth(-1),
	m_prevHeight(-1)
{
	assert(pComp);

	// TODO: for debugging
	//initGLFunctions();

	QObject::connect(this, &MIPQt5OutputWindow::signalInternalNewFrame, this, &MIPQt5OutputWindow::slotInternalNewFrame);
	pComp->registerWindow(this);

	// TODO: for testing, to see if drawing changes when no data comes in
#if 0
	QTimer *pTimer = new QTimer(this);	
	QObject::connect(pTimer, &QTimer::timeout, this, &QWindow::requestUpdate);
	pTimer->setInterval(2000);
	pTimer->start();
#endif
}

MIPQt5OutputWindow::~MIPQt5OutputWindow()
{
	m_mutex.lock();
	if (m_pComponent)
	{
		m_pComponent->unregisterWindow(this);
		m_pComponent = nullptr;
	}
	m_mutex.unlock();
}

void MIPQt5OutputWindow::clearComponent()
{
	m_mutex.lock();
	m_pComponent = nullptr;
	m_mutex.unlock();
}

void MIPQt5OutputWindow::slotInternalNewFrame(MIPVideoMessage *pMsg)
{
	// Here, the data in pMsg is received in the main qt eventloop again

	if (m_pFrameToProcess)
		delete m_pFrameToProcess;
	m_pFrameToProcess = pMsg;

	checkDisplayRoutines();

	requestUpdate();
}

void MIPQt5OutputWindow::injectFrame(MIPVideoMessage *pMsg)
{
	emit signalInternalNewFrame(pMsg);
}

void MIPQt5OutputWindow::initializeGL()
{
	initializeOpenGLFunctions();

	//cerr << "Context for " << getSourceID() << " = " << (void *)test_glXGetCurrentContext() << endl;

	float vertices[] = { -1.0f, -1.0f,
		                 -1.0f, 1.0f,
						 1.0f, -1.0f,
						 1.0f, 1.0f };

	float texCorners[] = { 0.0f, 1.0f,
		                   0.0f, 0.0f,
						   1.0f, 1.0f,
						   1.0f, 0.0f };

	GLuint vBuf;
	glGenBuffers(1, &vBuf);
	glBindBuffer(GL_ARRAY_BUFFER, vBuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	GLuint tBuf;
	glGenBuffers(1, &tBuf);
	glBindBuffer(GL_ARRAY_BUFFER, tBuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texCorners), texCorners, GL_STATIC_DRAW);

	glActiveTexture(GL_TEXTURE0);
	m_tex0 = createTexture();

	glActiveTexture(GL_TEXTURE1);
	m_tex1 = createTexture();

	// YUV420
	{
		m_pYUV420Program = new QOpenGLShaderProgram(this);
		m_pYUV420Program->addShaderFromSourceCode(QOpenGLShader::Vertex, s_vertexShader);
		m_pYUV420Program->addShaderFromSourceCode(QOpenGLShader::Fragment, s_fragmentShaderYUV420);
		m_pYUV420Program->link();
		m_pYUV420Program->bind();

		glBindBuffer(GL_ARRAY_BUFFER, vBuf);
		auto posLoc = m_pYUV420Program->attributeLocation("a_position");
		glEnableVertexAttribArray(posLoc);
		glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ARRAY_BUFFER, tBuf);
		auto tposLoc = m_pYUV420Program->attributeLocation("a_texcoord");
		glEnableVertexAttribArray(tposLoc);
		glVertexAttribPointer(tposLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

		auto yTexLoc = m_pYUV420Program->uniformLocation("u_Ytex");
		auto uvTexLoc = m_pYUV420Program->uniformLocation("u_UVtex");

		glUniform1i(yTexLoc, 0); // Texture unit 0
		glUniform1i(uvTexLoc, 1); // Texture unit 1
	}

	// RGB
	{
		m_pRGBProgram = new QOpenGLShaderProgram(this);
		m_pRGBProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, s_vertexShader);
		m_pRGBProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, s_fragmentShaderRGB);
		m_pRGBProgram->link();
		m_pRGBProgram->bind();

		glBindBuffer(GL_ARRAY_BUFFER, vBuf);
		auto posLoc = m_pRGBProgram->attributeLocation("a_position");
		glEnableVertexAttribArray(posLoc);
		glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ARRAY_BUFFER, tBuf);
		auto tposLoc = m_pRGBProgram->attributeLocation("a_texcoord");
		glEnableVertexAttribArray(tposLoc);
		glVertexAttribPointer(tposLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

		auto texLoc = m_pRGBProgram->uniformLocation("u_tex");
		glUniform1i(texLoc, 0); // Texture unit 0
	}
	
	// YUYV
	{
		m_pYUYVProgram = new QOpenGLShaderProgram(this);
		m_pYUYVProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, s_vertexShader);
		m_pYUYVProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, s_fragmentShaderYUYV);
		m_pYUYVProgram->link();
		m_pYUYVProgram->bind();

		glBindBuffer(GL_ARRAY_BUFFER, vBuf);
		auto posLoc = m_pYUYVProgram->attributeLocation("a_position");
		glEnableVertexAttribArray(posLoc);
		glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ARRAY_BUFFER, tBuf);
		auto tposLoc = m_pYUYVProgram->attributeLocation("a_texcoord");
		glEnableVertexAttribArray(tposLoc);
		glVertexAttribPointer(tposLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

		auto texLoc = m_pYUYVProgram->uniformLocation("u_tex");
		glUniform1i(texLoc, 0); // Texture unit 0
		
		m_widthLoc = m_pYUYVProgram->uniformLocation("u_width");
		glUniform1f(m_widthLoc, 1.0);
	}

	m_init = true;
}

GLuint MIPQt5OutputWindow::createTexture()
{
	GLuint t;

	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return t;
}

void MIPQt5OutputWindow::checkDisplayRoutines()
{
	if (!m_init)
		return;

	makeCurrent(); // if it's called from the slotInternalNewFrame function, the context needs to be activated!

	if (m_pFrameToProcess)
	{
		MIPVideoMessage *pMsg = m_pFrameToProcess;
		unique_ptr<MIPVideoMessage> dummy(pMsg); // to delete it automatically later on

		m_pFrameToProcess = nullptr;
		const int w = pMsg->getWidth();
		const int h = pMsg->getHeight();

		if (w != m_prevWidth)
			emit signalResizeWidth(w);
		if (h != m_prevHeight)
			emit signalResizeHeight(h);

		if (w != m_prevWidth || h != m_prevHeight)
		{
			//cerr << "Emitting resize signal for " << getSourceID() << endl;
			emit signalResize(w, h);
		}

		m_prevWidth = w;
		m_prevHeight = h;

		//cerr << "Upload context for " << getSourceID() << " = " << (void *)test_glXGetCurrentContext() << endl;

		if (pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUV420P)
		{
			MIPRawYUV420PVideoMessage *pYUVMsg = static_cast<MIPRawYUV420PVideoMessage *>(pMsg);
			m_pYUV420Program->bind();
			m_pLastUsedProgram = m_pYUV420Program;

			const uint8_t *pAllData = pYUVMsg->getImageData();
			const uint8_t *pYData = pAllData;
			const uint8_t *pUVData = pAllData+w*h;

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_tex0);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pYData);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, m_tex1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w/2, h, 0, GL_RED, GL_UNSIGNED_BYTE, pUVData);
		}
		else if (pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && 
				 (pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_RGB32 || pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_RGB24) )
		{
			MIPRawRGBVideoMessage *pRGBMsg = static_cast<MIPRawRGBVideoMessage *>(pMsg);
			m_pRGBProgram->bind();
			m_pLastUsedProgram = m_pRGBProgram;

			const uint8_t *pAllData = pRGBMsg->getImageData();
			GLint fmt = (pRGBMsg->is32Bit())?GL_RGBA:GL_RGB;

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_tex0);
			glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, pAllData);
		}
		else if (pMsg->getMessageType() == MIPMESSAGE_TYPE_VIDEO_RAW && pMsg->getMessageSubtype() == MIPRAWVIDEOMESSAGE_TYPE_YUYV)
		{
			MIPRawYUYVVideoMessage *pYUYVMsg = static_cast<MIPRawYUYVVideoMessage *>(pMsg);
			m_pYUYVProgram->bind();
			m_pLastUsedProgram = m_pYUYVProgram;

			const uint8_t *pAllData = pYUYVMsg->getImageData();

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_tex0);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, w, h, 0, GL_RG, GL_UNSIGNED_BYTE, pAllData);
		
			glUniform1f(m_widthLoc, (float)w);
		}
	}
}

void MIPQt5OutputWindow::paintGL()
{
	checkDisplayRoutines();

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void MIPQt5OutputWindow::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
}

// Object

MIPQt5OutputObject::MIPQt5OutputObject()
{
}

MIPQt5OutputObject::~MIPQt5OutputObject()
{
}

void MIPQt5OutputObject::callNewSource(quint64 sourceID)
{
	emit signalNewSource(sourceID);
}

void MIPQt5OutputObject::callRemovedSource(quint64 sourceID)
{
	emit signalRemovedSource(sourceID);
}

// Component

MIPQt5OutputComponent::MIPQt5OutputComponent() : MIPComponent("MIPQt5OutputComponent"), m_pSignalObject(nullptr)
{
	int status;
	
	if ((status = m_mutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize Qt5 component mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPQt5OutputComponent::~MIPQt5OutputComponent()
{
	destroy();
}

bool MIPQt5OutputComponent::init()
{
	if (m_pSignalObject)
	{
		setErrorString("TODO: already init");
		return false;
	}

	m_pSignalObject = new MIPQt5OutputObject();
	return true;
}

bool MIPQt5OutputComponent::destroy()
{
	if (!m_pSignalObject)
	{
		setErrorString("TODO: not init");
		return false;
	}

	delete m_pSignalObject;
	m_pSignalObject = 0;

	// Make sure that when the widget is destroyed, this component is no longer contacted
	for (auto pWindow : m_windows)
		pWindow->clearComponent();

	return true;
}

MIPQt5OutputObject *MIPQt5OutputComponent::getSignallingObject()
{
	if (!m_pSignalObject)
	{
		setErrorString("TODO: not init");
		return nullptr;
	}
	return m_pSignalObject;
}

MIPQt5OutputWindow *MIPQt5OutputComponent::createWindow(uint64_t sourceID)
{
	MIPQt5OutputWindow *pWindow = nullptr;

	pWindow = new MIPQt5OutputWindow(this, sourceID); // This registers itself with this instance
	return pWindow;
}

bool MIPQt5OutputComponent::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	assert(pMsg);

	if (pMsg->getMessageType() != MIPMESSAGE_TYPE_VIDEO_RAW)
	{
		setErrorString("TODO: bad msg");
		return false;
	}

	// TODO: check subtype

	// Find the window and just send a copy of the message, let the window handle it
	MIPVideoMessage *pVidMsg = static_cast<MIPVideoMessage *>(pMsg);
	uint64_t sourceID = pVidMsg->getSourceID();

	m_mutex.Lock();
	for (auto it = m_windows.begin() ; it != m_windows.end() ; it++)
	{
		MIPQt5OutputWindow *pWin = *it;
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
		m_pSignalObject->callNewSource(sourceID);
	}
	else
		it->second = now;

	// TODO: make this configurable
	double timeout = 10.0; // remove stream if inactive for a certain period

	it = m_sourceTimes.begin();
	while (it != m_sourceTimes.end())
	{
		if (now.getValue() - it->second.getValue() > timeout)
		{
			uint64_t sourceID = it->first;
			auto it2 = it;
			++it;
			m_sourceTimes.erase(it2);

			// Remove sourceID from m_windows
			m_mutex.Lock();
			auto iw = m_windows.begin();
			while (iw != m_windows.end())
			{
				MIPQt5OutputWindow *pWin = *iw;
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
			m_mutex.Unlock();

			m_pSignalObject->callRemovedSource(sourceID);
		}
		else
			++it;
	}
	
	return true;
}

bool MIPQt5OutputComponent::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString("TODO: no pull");
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

// MDI test widget

MIPQt5OutputMDIWidget::MIPQt5OutputMDIWidget(MIPQt5OutputComponent *pComp) : m_pComponent(pComp)
{
	MIPQt5OutputObject *pObj = nullptr;
	if (pComp)
		pObj = pComp->getSignallingObject();

	if (pObj)
	{
		QObject::connect(pObj, &MIPQt5OutputObject::signalNewSource, this, &MIPQt5OutputMDIWidget::slotNewSource);
		QObject::connect(pObj, &MIPQt5OutputObject::signalRemovedSource, this, &MIPQt5OutputMDIWidget::slotRemovedSource);
	}

	m_pMdiArea = new QMdiArea(this);
	setCentralWidget(m_pMdiArea);
}

MIPQt5OutputMDIWidget::~MIPQt5OutputMDIWidget()
{
}

void MIPQt5OutputMDIWidget::slotResize(int w, int h)
{
	MIPQt5OutputWindow *pWin = qobject_cast<MIPQt5OutputWindow*>(sender());
	if (!pWin)
	{
		cerr << "Warning: got resize signal, but couldn't get window" << endl;
		return;
	}
	//cout << "Resizing " << pWin->getSourceID() << " to " << w << "x" << h << endl;
	QString sourceStr = QString::number(pWin->getSourceID());

	for (auto &pSubWin : m_pMdiArea->subWindowList())
	{
		if (pSubWin->windowTitle() == sourceStr)
			pSubWin->resize(w, h);
	}
}

void MIPQt5OutputMDIWidget::slotNewSource(quint64 sourceID)
{
	MIPQt5OutputWindow *pWin = m_pComponent->createWindow(sourceID);
	QString sourceStr = QString::number(sourceID);

	QWidget *pWidget = QWidget::createWindowContainer(pWin, m_pMdiArea);
	pWidget->show();

	QMdiSubWindow *pMdiWin = new QMdiSubWindow(m_pMdiArea);
	pMdiWin->setWidget(pWidget);
	pMdiWin->setWindowTitle(sourceStr);
	pMdiWin->setAttribute(Qt::WA_DeleteOnClose);
	pMdiWin->show();

	QObject::connect(pWin, &MIPQt5OutputWindow::signalResize, this, &MIPQt5OutputMDIWidget::slotResize);
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

