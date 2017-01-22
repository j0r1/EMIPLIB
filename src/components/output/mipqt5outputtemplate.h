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


OUTPUTCLASSNAME::OUTPUTCLASSNAME(MIPQt5OutputComponent *pComp, uint64_t sourceID) 
	: PARENTCLASSNAME(), 
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

	QObject::connect(this, &OUTPUTCLASSNAME::signalInternalNewFrame, this, &OUTPUTCLASSNAME::slotInternalNewFrame);
	pComp->registerWindow(this);
}

OUTPUTCLASSNAME::~OUTPUTCLASSNAME()
{
	m_mutex.lock();
	if (m_pComponent)
	{
		m_pComponent->unregisterWindow(this);
		m_pComponent = nullptr;
	}
	m_mutex.unlock();
}

void OUTPUTCLASSNAME::clearComponent()
{
	m_mutex.lock();
	m_pComponent = nullptr;
	m_mutex.unlock();
}

int OUTPUTCLASSNAME::getVideoWidth()
{
	m_mutex.lock();
	int w = m_prevWidth;
	m_mutex.unlock();
	return w;
}

int OUTPUTCLASSNAME::getVideoHeight()
{
	m_mutex.lock();
	int h = m_prevHeight;
	m_mutex.unlock();
	return h;
}

void OUTPUTCLASSNAME::slotInternalNewFrame(MIPVideoMessage *pMsg)
{
	// Here, the data in pMsg is received in the main qt eventloop again

	if (m_pFrameToProcess)
		delete m_pFrameToProcess;
	m_pFrameToProcess = pMsg;

	checkDisplayRoutines();

	REQUESTUPDATE;
}

void OUTPUTCLASSNAME::injectFrame(MIPVideoMessage *pMsg)
{
	emit signalInternalNewFrame(pMsg);
}

void OUTPUTCLASSNAME::initializeGL()
{
	initializeOpenGLFunctions();

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
		auto ret = m_pYUV420Program->link();
		if (!ret)
			qDebug() << "Error linking program: " << m_pYUV420Program->log();

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

GLuint OUTPUTCLASSNAME::createTexture()
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

void OUTPUTCLASSNAME::checkDisplayRoutines()
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

		m_mutex.lock();
		m_prevWidth = w;
		m_prevHeight = h;
		m_mutex.unlock();

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
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pYData);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, m_tex1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w/2, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pUVData);
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
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w/2, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pAllData);
		
			glUniform1f(m_widthLoc, (float)w);
		}
	}
}

void OUTPUTCLASSNAME::paintGL()
{
	checkDisplayRoutines();

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void OUTPUTCLASSNAME::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
}

