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

/**
 * \file mipyuv420fileinput.h
 */

#ifndef MIPYUV420FILEINPUT_H

#define MIPYUV420FILEINPUT_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include <stdio.h>

class MIPRawYUV420PVideoMessage;

/** Read uncompressed YUV420P frames from a file.
 *  This component allow you to read uncompressed, raw YUV420P frames
 *  from a file. It only accepts MIPSYSTEMMESSAGE_TYPE_ISTIME system
 *  messages as input, and produces a raw YUV420P video message as
 *  output.
 */
class EMIPLIB_IMPORTEXPORT MIPYUV420FileInput : public MIPComponent
{
public:
	MIPYUV420FileInput();
	~MIPYUV420FileInput();

	/** Opens a file.
	 *  This method opens a file and will assume that each frame has 
	 *  a specific width and height. Each time the \c push method
	 *  is called, this will result in (width*height*3)/2 bytes
	 *  being read from the file, and being interpreted as the data
	 *  of a single YUV frame.
	 */
	bool open(const std::string &fileName, int width, int height);

	/** Closes the file again and deinitiazes the component. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	int m_width, m_height;
	int m_frameSize;
	FILE *m_pFile;
	uint8_t *m_pData;
	MIPRawYUV420PVideoMessage *m_pVideoMessage;
	bool m_gotMessage;
};

#endif // MIPYUV420FILEINPUT_H
