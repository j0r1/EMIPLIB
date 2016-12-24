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

#include "mipconfig.h"
#include "mipyuv420fileinput.h"
#include "miprawvideomessage.h"
#include "mipsystemmessage.h"

#define MIPYUV420FILEINPUT_ERRSTR_ALREADYOPEN			"A file has already been opened"
#define MIPYUV420FILEINPUT_ERRSTR_NOTOPEN			"No file has been opened yet"
#define MIPYUV420FILEINPUT_ERRSTR_BADMESSAGE			"Only a MIPSYSTEMMESSAGE_TYPE_ISTIME system message is allowed as input"
#define MIPYUV420FILEINPUT_ERRSTR_CANTOPEN			"Unable to open the specified file"
#define MIPYUV420FILEINPUT_ERRSTR_INVALIDDIMENSION		"An invalid width or height was specified"
#define MIPYUV420FILEINPUT_ERRSTR_DIMENSIONNOTEVEN		"Both width and height should be even numbers"

MIPYUV420FileInput::MIPYUV420FileInput() : MIPComponent("MIPYUV420FileInput")
{
	m_pFile = 0;
	m_width = 0;
	m_height = 0;
	m_frameSize = 0;
	m_pData = 0;
	m_pVideoMessage = 0;
	m_gotMessage = false;
}

MIPYUV420FileInput::~MIPYUV420FileInput()
{
	close();
}

bool MIPYUV420FileInput::open(const std::string &fileName, int width, int height)
{
	if (m_pFile != 0)
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_ALREADYOPEN);
		return false;
	}

	if (width < 1 || height < 1)
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_INVALIDDIMENSION);
		return false;
	}

	if ((width%2 != 0) || (height%2 != 0))
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_DIMENSIONNOTEVEN);
		return false;
	}

	FILE *pFile = fopen(fileName.c_str(), "rb");
	if (pFile == 0)
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_CANTOPEN);
		return false;
	}

	m_pFile = pFile;
	m_width = width;
	m_height = height;
	m_frameSize = (width*height*3)/2;
	m_pData = new uint8_t[m_frameSize];
	m_pVideoMessage = new MIPRawYUV420PVideoMessage(width, height, m_pData, false);
	m_gotMessage = false;

	return true;
}

bool MIPYUV420FileInput::close()
{
	if (m_pFile == 0)
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_NOTOPEN);
		return false;
	}

	fclose(m_pFile);
	delete m_pVideoMessage;
	delete [] m_pData;

	m_width = 0;
	m_height = 0;
	m_frameSize = 0;
	m_pFile = 0;
	m_pData = 0;
	m_pVideoMessage = 0;

	return true;
}

bool MIPYUV420FileInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if ( !(pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_ISTIME) )
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	if (m_pFile == 0)
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_NOTOPEN);
		return false;
	}

	bool done = false;
	bool rewound = false;

	while (!done)
	{
		int num = fread(m_pData, 1, m_frameSize, m_pFile);

		if (num <= 0)
		{
			if (!rewound)
			{
				rewound = true;
				fseek(m_pFile, 0, SEEK_SET);
			}
			else
			{
				memset(m_pData, 0, m_frameSize);
				done = true;
			}
		}
		else if (num < m_frameSize)
		{
			memset(m_pData+num, 0, (m_frameSize-num));
			fseek(m_pFile, 0, SEEK_SET);
			done = true;
		}
		else // read the correct amount
		{
			done = true;
		}
	}

	m_gotMessage = false;

	return true;
}

bool MIPYUV420FileInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pFile == 0)
	{
		setErrorString(MIPYUV420FILEINPUT_ERRSTR_NOTOPEN);
		return false;
	}

	if (!m_gotMessage)
	{
		m_gotMessage = true;
		*pMsg = m_pVideoMessage;
	}
	else
	{
		m_gotMessage = false;
		*pMsg = 0;
	}

	return true;
}

