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
 * \file mipwavwriter.h
 */

#ifndef MIPWAVWRITER_H

#define MIPWAVWRITER_H

#include "mipconfig.h"
#include "miperrorbase.h"
#include "miptypes.h"
#include <stdio.h>

/** This is a simple WAV file writer (8 bit, mono) */
class EMIPLIB_IMPORTEXPORT MIPWAVWriter : public MIPErrorBase
{
public:
	MIPWAVWriter();
	~MIPWAVWriter();

	/** Opens the WAV file specified in \c fileName at sampling rate \c sampRate */
	bool open(const std::string &fileName, int sampRate);
	
	/** Closes the WAV file */
	bool close();

	/** Writes a number of frames.
	 *  This function allows you to write sound data to the file. Note that
	 *  currently only mono sound is supported.
	 *  \param pBuffer Sound data
	 *  \param numFrames Number of frames in the buffer. Is currently equal to the number of bytes.
	 */
	bool writeFrames(uint8_t *pBuffer, int numFrames);
private:
	FILE *m_pFile;
	int64_t m_frameCount;
};

#endif // MIPWAVWRITER_H

