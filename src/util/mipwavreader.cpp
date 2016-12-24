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
#include "mipwavreader.h"

#include "mipdebug.h"

#define MIPWAVREADER_ERRSTR_NOTOPENED				"No file was opened"
#define MIPWAVREADER_ERRSTR_ALREADYOPEN				"A file is already open"
#define MIPWAVREADER_ERRSTR_CANTSEEK				"Can't seek in file"
#define MIPWAVREADER_ERRSTR_CANTOPEN				"Can't open file"
#define MIPWAVREADER_ERRSTR_CANTREADRIFFID			"Can't read RIFF ID"
#define MIPWAVREADER_ERRSTR_BADRIFFID				"The file doesn't contain a RIFF identifier"
#define MIPWAVREADER_ERRSTR_CANTREADRIFFCHUNKSIZE		"Can't read RIFF chunk size"
#define MIPWAVREADER_ERRSTR_CANTREADWAVEID			"Can't read WAVE ID"
#define MIPWAVREADER_ERRSTR_RIFFCHUNKSIZETOOSMALL		"RIFF chunk size too small"
#define MIPWAVREADER_ERRSTR_NOFORMATCHUNKFOUND			"No format chunk found"
#define MIPWAVREADER_ERRSTR_CANTREADCHUNKID			"Can't read chunk ID"
#define MIPWAVREADER_ERRSTR_CANTREADCHUNKSIZE			"Can't read chunk size"
#define MIPWAVREADER_ERRSTR_CANTREADFORMATDATA			"Can't read format data"
#define MIPWAVREADER_ERRSTR_BADWAVEID				"No WAVE identifier was found"
#define MIPWAVREADER_ERRSTR_NOTPCMDATA				"Only uncompressed, PCM data is supported"
#define MIPWAVREADER_ERRSTR_MULTIPLEFORMATCHUNKS		"File contains multiple format chunks"
#define MIPWAVREADER_ERRSTR_MULTIPLEDATACHUNKS			"File contains multiple data chunks"
#define MIPWAVREADER_ERRSTR_SAMPLINGRATETOOHIGH			"Sampling rate too high"
#define MIPWAVREADER_ERRSTR_BADFRAMECOUNT			"A non-integer frame count was calculated"
#define MIPWAVREADER_ERRSTR_UNSUPPORTEDBITSPERSAMPLE		"Only 8, 16, 24 and 32 bit samples are supported"
#define MIPWAVREADER_ERRSTR_UNEXPECTEDEOF			"Couldn't read as much data as expected"

#define MIPWAVREADER_FRAMEBUFSIZE				4096

MIPWAVReader::MIPWAVReader()
{
	m_file = 0;
}

MIPWAVReader::~MIPWAVReader()
{
	close();
}

bool MIPWAVReader::open(const std::string &fileName)
{
	if (m_file != 0)
	{
		setErrorString(MIPWAVREADER_ERRSTR_ALREADYOPEN);
		return false;
	}

	FILE *f;

#if (defined(WIN32) && (!defined(_WIN32_WCE))) && (defined(_MSC_VER) && _MSC_VER >= 1400)
	if (fopen_s(&f, fileName.c_str(), "rb") != 0)
#else
	if ((f = fopen(fileName.c_str(),"rb")) == 0)
#endif 
	{
		setErrorString(MIPWAVREADER_ERRSTR_CANTOPEN);
		return false;
	}

	uint8_t riffID[4];

	if (fread(riffID, 1, 4, f) != 4)
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_CANTREADRIFFID);
		return false;
	}

	if (!(riffID[0] == 'R' && riffID[1] == 'I' && riffID[2] == 'F' && riffID[3] == 'F'))
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_BADRIFFID);
		return false;
	}
	
	uint8_t riffChunkSizeBytes[4];
	int64_t riffChunkSize;
	
	if (fread(riffChunkSizeBytes, 1, 4, f) != 4)
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_CANTREADRIFFCHUNKSIZE);
		return false;
	}

	riffChunkSize = (int64_t)((uint32_t)riffChunkSizeBytes[0] | (((uint32_t)riffChunkSizeBytes[1]) << 8) | (((uint32_t)riffChunkSizeBytes[2]) << 16) | (((uint32_t)riffChunkSizeBytes[3]) << 24));
	
	if (riffChunkSize < 4)
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_RIFFCHUNKSIZETOOSMALL);
		return false;
	}
	
	m_samplingRate = -1;
	m_channels = -1;
	m_framesLeft = 0;
	m_totalFrames = 0;
	m_bytesPerSample = 1;
	m_dataStartPos = 0;
	m_scale = 0;
	m_frameSize = 1;
	
	uint8_t waveID[4];

	if (fread(waveID, 1, 4, f) != 4)
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_CANTREADWAVEID);
		return false;
	}
	riffChunkSize -= 4;
	
	if (!(waveID[0] == 'W' && waveID[1] == 'A' && waveID[2] == 'V' && waveID[3] == 'E'))
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_BADWAVEID);
		return false;
	}

	int64_t dataChunkSize = -1;
	bool gotFmt = false;
	
	// Here we'll check out the chunks
	while (riffChunkSize > 0)
	{
		uint8_t chunkID[4];
		uint8_t chunkSizeBytes[4];
		int64_t chunkSize;
		
		if (fread(chunkID, 1, 4, f) != 4)
		{
			fclose(f);
			setErrorString(MIPWAVREADER_ERRSTR_CANTREADCHUNKID);
			return false;
		}
		riffChunkSize -= 4;	

		if (fread(chunkSizeBytes, 1, 4, f) != 4)
		{
			fclose(f);
			setErrorString(MIPWAVREADER_ERRSTR_CANTREADCHUNKSIZE);
			return false;
		}
		riffChunkSize -= 4;
		
		chunkSize = (int64_t)((uint32_t)chunkSizeBytes[0] | (((uint32_t)chunkSizeBytes[1]) << 8) | (((uint32_t)chunkSizeBytes[2]) << 16) | (((uint32_t)chunkSizeBytes[3]) << 24));

		if (chunkID[0] == 'f' && chunkID[1] == 'm' && chunkID[2] == 't' && chunkID[3] == ' ')
		{
			if (gotFmt)
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_MULTIPLEFORMATCHUNKS);
				return false;
			}
			
			uint8_t fmtData[16];

			if (fread(fmtData, 1, 16, f) != 16)
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_CANTREADFORMATDATA);
				return false;
			}
			riffChunkSize -= 16;

			if (!(fmtData[0] == 1 && fmtData[1] == 0))
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_NOTPCMDATA);
				return false;
			}

			m_channels = (int)(((uint16_t)fmtData[2]) | (((uint16_t)fmtData[3]) << 8));

			if (fmtData[7] != 0)
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_SAMPLINGRATETOOHIGH);
				return false;
			}

			m_samplingRate = (int)(((uint16_t)fmtData[4]) | (((uint16_t)fmtData[5]) << 8) | (((uint16_t)fmtData[6]) << 16));

			int bitsPerSample = (int)(((uint16_t)fmtData[14]) | (((uint16_t)fmtData[15]) << 8));

			if (!(bitsPerSample == 8 || bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32))
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_UNSUPPORTEDBITSPERSAMPLE);
				return false;
			}

			m_bytesPerSample = bitsPerSample/8;
			m_scale = (float)(2.0/((float)(((uint64_t)1) << bitsPerSample)));

			if (fseek(f,(long)(chunkSize-16),SEEK_CUR) == -1)
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_CANTSEEK);
				return false;
			}
			riffChunkSize -= (chunkSize-16);
		}
		else if (chunkID[0] == 'd' && chunkID[1] == 'a' && chunkID[2] == 't' && chunkID[3] == 'a')
		{
			if (dataChunkSize >= 0)
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_MULTIPLEDATACHUNKS);
				return false;
			}
					
			m_dataStartPos = ftell(f);
			dataChunkSize = chunkSize;

			if (fseek(f,(long)chunkSize,SEEK_CUR) == -1)
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_CANTSEEK);
				return  false;
			}
			riffChunkSize -= chunkSize;
		}
		else // unsupported chunk type, just skip it
		{
			if (fseek(f,(long)chunkSize,SEEK_CUR) == -1)
			{
				fclose(f);
				setErrorString(MIPWAVREADER_ERRSTR_CANTSEEK);
				return  false;
			}
			riffChunkSize -= chunkSize;
		}
	}

	if (m_samplingRate < 0 || m_channels < 0)
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_NOFORMATCHUNKFOUND);
		return false;
	}
	
	if (fseek(f,m_dataStartPos,SEEK_SET) == -1)
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_CANTSEEK);
		return false;
	}

	m_frameSize = m_bytesPerSample*m_channels;

	if ((dataChunkSize % m_frameSize) != 0)
	{
		fclose(f);
		setErrorString(MIPWAVREADER_ERRSTR_BADFRAMECOUNT);
		return false;
	}

	m_totalFrames = dataChunkSize / m_frameSize;
	m_framesLeft = m_totalFrames;
	m_pFrameBuffer = new uint8_t [m_frameSize*MIPWAVREADER_FRAMEBUFSIZE];
	
	if (m_bytesPerSample == 4)
		m_negStartVal = 0x00000000;
	else if (m_bytesPerSample == 3)
		m_negStartVal = 0xff000000;
	else if (m_bytesPerSample == 2)
		m_negStartVal = 0xffff0000;
	else
		m_negStartVal = 0xffffff00;

	m_file = f;
	return true;
}

bool MIPWAVReader::close()
{
	if (m_file == 0)
	{
		setErrorString(MIPWAVREADER_ERRSTR_NOTOPENED);
		return false;
	}

	delete [] m_pFrameBuffer;
	fclose(m_file);
	m_file = 0;
	return true;
}

bool MIPWAVReader::readFrames(float *buffer, int numFrames, int *numFramesRead)
{
	if (m_file == 0)
	{
		setErrorString(MIPWAVREADER_ERRSTR_NOTOPENED);
		return false;
	}

	int64_t framesToRead = numFrames;
	int64_t framesRead = 0;

	if (framesToRead > m_framesLeft)
		framesToRead = m_framesLeft;
	
	int floatBufPos = 0;
	
	while (framesToRead > 0)
	{
		int num = (int)framesToRead;
		
		if (num > MIPWAVREADER_FRAMEBUFSIZE)
			num = MIPWAVREADER_FRAMEBUFSIZE;

		if ((int)fread(m_pFrameBuffer, m_frameSize, num, m_file) != num)
		{
			setErrorString(MIPWAVREADER_ERRSTR_UNEXPECTEDEOF);
			return false;
		}

		int byteBufPos = 0;
		
		for (int i = 0 ; i < num ; i++)
		{
			for (int j = 0 ; j < m_channels ; j++)
			{
				if (m_bytesPerSample == 1)
				{
					buffer[floatBufPos] = ((float)(m_pFrameBuffer[byteBufPos])-128.0f)*m_scale;
					byteBufPos++;
				}
				else
				{
					uint32_t x = 0;
					
					if ((m_pFrameBuffer[byteBufPos + m_bytesPerSample - 1] & 0x80) == 0x80)
						x = m_negStartVal;
	
						int shiftNum = 0;
					for (int k = 0 ; k < m_bytesPerSample ; k++, shiftNum += 8, byteBufPos++)
						x |= ((uint32_t)(m_pFrameBuffer[byteBufPos])) << shiftNum;

					int32_t y = *((int32_t *)(&x));

					buffer[floatBufPos] = ((float)y)*m_scale;
				}		
				floatBufPos++;
			}
		}
		
		framesToRead -= num;
		framesRead += num;
	}

	*numFramesRead = (int)framesRead;
	m_framesLeft -= framesRead;
	
	return true;
}

bool MIPWAVReader::readFrames(int16_t *buffer, int numFrames, int *numFramesRead)
{
	if (m_file == 0)
	{
		setErrorString(MIPWAVREADER_ERRSTR_NOTOPENED);
		return false;
	}

	int64_t framesToRead = numFrames;
	int64_t framesRead = 0;

	if (framesToRead > m_framesLeft)
		framesToRead = m_framesLeft;
	
	int intBufPos = 0;
	
	while (framesToRead > 0)
	{
		int num = (int)framesToRead;
		
		if (num > MIPWAVREADER_FRAMEBUFSIZE)
			num = MIPWAVREADER_FRAMEBUFSIZE;

		if ((int)fread(m_pFrameBuffer, m_frameSize, num, m_file) != num)
		{
			setErrorString(MIPWAVREADER_ERRSTR_UNEXPECTEDEOF);
			return false;
		}

		int byteBufPos = 0;
		
		for (int i = 0 ; i < num ; i++)
		{
			for (int j = 0 ; j < m_channels ; j++)
			{
				if (m_bytesPerSample == 1)
				{
					buffer[intBufPos] = (((int16_t)(m_pFrameBuffer[byteBufPos])-128)<<8);
					byteBufPos++;
				}
				else
				{		
					uint32_t x = 0;

					if ((m_pFrameBuffer[byteBufPos + m_bytesPerSample - 1] & 0x80) == 0x80)
						x = m_negStartVal;
		
					int shiftNum = 0;
					for (int k = 0 ; k < m_bytesPerSample ; k++, shiftNum += 8, byteBufPos++)
						x |= ((uint32_t)(m_pFrameBuffer[byteBufPos])) << shiftNum;

					int32_t y = *((int32_t *)(&x));

					if (m_bytesPerSample == 2)
						buffer[intBufPos] = (int16_t)y;
					else if (m_bytesPerSample == 3)
						buffer[intBufPos] = (int16_t)(y >> 8);
					else
						buffer[intBufPos] = (int16_t)(y >> 16);
				}	
				intBufPos++;
			}
		}
		
		framesToRead -= num;
		framesRead += num;
	}

	*numFramesRead = (int)framesRead;
	m_framesLeft -= framesRead;
	
	return true;
}

bool MIPWAVReader::rewind()
{
	if (m_file == 0)
	{
		setErrorString(MIPWAVREADER_ERRSTR_NOTOPENED);
		return false;
	}

	if (fseek(m_file,m_dataStartPos,SEEK_SET) == -1)
	{
		setErrorString(MIPWAVREADER_ERRSTR_CANTSEEK);
		return false;
	}

	m_framesLeft = m_totalFrames;
	return true;
}

