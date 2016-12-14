/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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
 * \file mipwavreader.h
 */

#ifndef MIPWAVREADER_H

#define MIPWAVREADER_H

#include "mipconfig.h"
#include "miperrorbase.h"
#include "miptypes.h"
#include <stdio.h>

/** This is a simple WAV file reader. */
class MIPWAVReader : public MIPErrorBase
{
public:
	MIPWAVReader();
	~MIPWAVReader();

	/** Opens the WAV file specified in \c fileName. */
	bool open(const std::string &fileName);

	/** Returns the sampling rate. */
	int getSamplingRate() const									{ return m_samplingRate; }

	/** Returns the number of channels. */
	int getNumberOfChannels() const									{ return m_channels; }

	/** Returns the number of frames in the file. */
	int64_t getNumberOfFrames() const								{ return m_totalFrames; }

	/** Read a number of frames.
	 *  Using this function, a number of frames can be read from a file.
	 *  \param pBuffer Buffer in which the read frames will be stored.
	 *  \param numFrames The number of frames to read.
	 *  \param numFramesRead Used to store the actual number of frames read.
	 */
	bool readFrames(float *pBuffer, int numFrames, int *numFramesRead);

	/** Read a number of frames.
	 *  Using this function, a number of frames can be read from a file.
	 *  \param pBuffer Buffer in which the read frames will be stored.
	 *  \param numFrames The number of frames to read.
	 *  \param numFramesRead Used to store the actual number of frames read.
	 */
	bool readFrames(int16_t *pBuffer, int numFrames, int *numFramesRead);

	/** This function resets the state of the file as if it was just opened. */
	bool rewind();

	/** Closes the file. */
	bool close();
private:
	FILE *m_file;
	int m_samplingRate;
	int m_channels;
	int m_frameSize;
	int64_t m_framesLeft;
	int64_t m_totalFrames;
	int m_bytesPerSample;
	long m_dataStartPos;
	float m_scale;
	uint8_t *m_pFrameBuffer;
	uint32_t m_negStartVal;
};

#endif // MIPWAVREADER_H

