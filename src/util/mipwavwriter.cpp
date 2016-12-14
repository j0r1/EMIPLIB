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
#include "mipwavwriter.h"
#include <string.h>

#include "mipdebug.h"

#define MIPWAVWRITER_ERRSTR_ALREADYOPEN				"A file is already open"
#define MIPWAVWRITER_ERRSTR_NOTOPEN				"No file was opened"
#define MIPWAVWRITER_ERRSTR_CANTOPENFILE			"Unable to open the specified file"
#define MIPWAVWRITER_ERRSTR_WRITEERROR				"Not all samples were successfully written to the file"

MIPWAVWriter::MIPWAVWriter()
{
	m_pFile = 0;
	m_frameCount = 0;
}

MIPWAVWriter::~MIPWAVWriter()
{
	close();
}

bool MIPWAVWriter::open(const std::string &fileName, int sampRate)
{
	if (m_pFile != 0)
	{
		setErrorString(MIPWAVWRITER_ERRSTR_ALREADYOPEN);
		return false;
	}

	m_pFile = fopen(fileName.c_str(),"wb");
	if (m_pFile == 0)
	{
		setErrorString(MIPWAVWRITER_ERRSTR_CANTOPENFILE);
		return false;
	}

	uint8_t tmp[16];

	memcpy(tmp, "RIFF", 4);
	fwrite(tmp, 1, 4, m_pFile);

	tmp[0] = 0; tmp[1] = 0; tmp[2] = 0; tmp[3] = 0;
	fwrite(tmp, 1, 4, m_pFile);

	memcpy(tmp, "WAVE", 4);
	fwrite(tmp, 1, 4, m_pFile);

	memcpy(tmp, "fmt ", 4);
	fwrite(tmp, 1, 4, m_pFile);

	tmp[0] = 16; tmp[1] = 0; tmp[2] = 0; tmp[3] = 0;
	fwrite(tmp, 1, 4, m_pFile);

	tmp[0] = 1; tmp[1] = 0;
	fwrite(tmp, 1, 2, m_pFile);
	fwrite(tmp, 1, 2, m_pFile);
	
	for (int i = 0 ; i < 4 ; i++)
		tmp[i] = (uint8_t)((sampRate>>(i*8))&0xff);
	fwrite(tmp, 1, 4, m_pFile);
	fwrite(tmp, 1, 4, m_pFile);

	tmp[0] = 1; tmp[1] = 0;
	fwrite(tmp, 1, 2, m_pFile);
	
	tmp[0] = 8; tmp[1] = 0;
	fwrite(tmp, 1, 2, m_pFile);

	memcpy(tmp, "data", 4);
	fwrite(tmp, 1, 4, m_pFile);

	tmp[0] = 0; tmp[1] = 0; tmp[2] = 0; tmp[3] = 0;
	fwrite(tmp, 1, 4, m_pFile);
	
	return true;
}

bool MIPWAVWriter::close()
{
	if (m_pFile == 0)
	{
		setErrorString(MIPWAVWRITER_ERRSTR_NOTOPEN);
		return false;
	}

	uint8_t tmp[16];
	int64_t fileSize = m_frameCount + 44; // 44 header bytes
	
	if (m_frameCount%2 != 0)
	{
		tmp[0] = 0;
		fwrite(tmp, 1, 1, m_pFile);
		fileSize++;
	}

	fseek(m_pFile, 4, SEEK_SET);
	fileSize -= 8; // don't count the first eight header bytes
	for (int i = 0 ; i < 4 ; i++)
		tmp[i] = (uint8_t)((fileSize>>(i*8))&0xff);
	fwrite(tmp, 1, 4, m_pFile);

	fseek(m_pFile, 40, SEEK_SET);
	for (int i = 0 ; i < 4 ; i++)
		tmp[i] = (uint8_t)((m_frameCount>>(i*8))&0xff);
	fwrite(tmp, 1, 4, m_pFile);
	
	fclose(m_pFile);
	m_pFile = 0;
	m_frameCount = 0;

	return true;
}

bool MIPWAVWriter::writeFrames(uint8_t *pBuffer, int numFrames)
{
	if (m_pFile == 0)
	{
		setErrorString(MIPWAVWRITER_ERRSTR_NOTOPEN);
		return false;
	}

	size_t numWritten = fwrite(pBuffer, 1, (size_t)numFrames, m_pFile);
	m_frameCount += (int64_t)numWritten;

	if ((int)numWritten != numFrames)
	{
		setErrorString(MIPWAVWRITER_ERRSTR_WRITEERROR);
		return false;
	}

	return true;
}

