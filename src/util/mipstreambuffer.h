/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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
 * \file mipstreambuffer.h
 */

#ifndef MIPSTREAMBUFFER_H

#define MIPSTREAMBUFFER_H

#include "mipconfig.h"
#include "miptypes.h"
#include <jmutex.h>
#include <list>
#include <vector>

/** This class represents a thread-safe buffer to which data can be
 *  written and from which the same data can be read again, making
 *  it well suited to safely pass data between threads.
 */
class MIPStreamBuffer
{
public:
	/** Constructs a MIPStreamBuffer object.
	 *  Constructs a MIPStreamBuffer object.
	 *  \param blockSize The data will be stored in a linked list containing
	 *                   blocks of this size.
	 *  \param preAllocBlocks The number of blocks for which memory will be
	 *                   pre-allocated.
	 */
	MIPStreamBuffer(int blockSize, int preAllocBlocks = 0);
	~MIPStreamBuffer();

	/** Resets the buffers. */
	void clear();

	/** Writes \c amount bytes from \c pData into the buffer. */
	void write(const void *pData, int amount);

	/** Reads and removes \c amount bytes from the buffer, storing them in \c pData. */
	int read(void *pData, int amount);

	/** Returns the amount of data which is currently being stored. */
	int getAmountBuffered() const;
private:
	class BufferBlock
	{
	public:
		BufferBlock(uint8_t *pData, int bytesFilled)			{ m_pData = pData; m_offset = 0; m_endPos = bytesFilled; }
		~BufferBlock()							{ delete [] m_pData; }
		uint8_t *getData() const					{ return m_pData; }
		void setOffset(int offset)					{ m_offset = offset; }
		int getOffset() const						{ return m_offset; }
		void setEndPos(int endPos)					{ m_endPos = endPos; }
		int getEndPos() const						{ return m_endPos; }
	private:
		uint8_t *m_pData;
		int m_offset;
		int m_endPos;
	};

	void releaseBlock(BufferBlock *pBlock);
	BufferBlock *acquireBlock();

	std::list<BufferBlock *> m_blocks;
	std::vector<BufferBlock *> m_preAllocatedBlocks;
	int m_blockSize;
	int m_bytesAvailable;
	int m_numPreAlloc;
	int m_numPreAllocAvailable;
	mutable JMutex m_mutex;
};

#endif // MIPSTREAMBUFFER_H

