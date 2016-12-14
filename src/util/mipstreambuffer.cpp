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

#include "mipconfig.h"
#include "mipstreambuffer.h"
#include <jmutexautolock.h>
#include <iostream>
#include <cstdlib>
#include <string.h>

#include "mipdebug.h"

MIPStreamBuffer::MIPStreamBuffer(int blockSize, int preAllocBlocks)
{
	int status;
	
	if ((status = m_mutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize component mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}

	m_blockSize = blockSize;
	m_bytesAvailable = 0;

	m_preAllocatedBlocks.resize(preAllocBlocks);
	for (int i = 0 ; i < preAllocBlocks ; i++)
	{
		uint8_t *pData = new uint8_t[blockSize];
		BufferBlock *pBlock = new BufferBlock(pData, 0);

		m_preAllocatedBlocks[i] = pBlock;
	}
	
	m_numPreAlloc = preAllocBlocks;
	m_numPreAllocAvailable = preAllocBlocks;
}

MIPStreamBuffer::~MIPStreamBuffer()
{
	clear();

	for (int i = 0 ; i < m_numPreAllocAvailable ; i++)
		delete m_preAllocatedBlocks[i];
}

void MIPStreamBuffer::clear()
{
	JMutexAutoLock autoLock(m_mutex);

	while (!m_blocks.empty())
	{
		releaseBlock(*(m_blocks.begin()));
		m_blocks.pop_front();
	}
	
	m_bytesAvailable = 0;
}

int MIPStreamBuffer::getAmountBuffered() const
{
	JMutexAutoLock autoLock(m_mutex);
	int num = m_bytesAvailable;
	return num;
}

void MIPStreamBuffer::write(const void *pData, int amount)
{
	JMutexAutoLock autoLock(m_mutex);

	const uint8_t *pPos = (const uint8_t *)pData;
	int numLeft = amount;
	BufferBlock *pLastBlock = 0;
	
	if (!m_blocks.empty())
	{
		std::list<BufferBlock *>::const_iterator it;

		it = m_blocks.end();
		--it;

		pLastBlock = (*it);
	}
	else
	{
		pLastBlock = acquireBlock();

		m_blocks.push_back(pLastBlock);
	}
	
	while (numLeft > 0)
	{
		int endPos = pLastBlock->getEndPos();
		int offset = pLastBlock->getOffset();
		int blockSpaceLeft = m_blockSize - endPos;
		int bytesToWrite = numLeft;
		uint8_t *pBlockData = pLastBlock->getData();
		
		if (bytesToWrite > blockSpaceLeft)
		{
			bytesToWrite = blockSpaceLeft;

			if (bytesToWrite > 0)
				memcpy(pBlockData + endPos, pPos, bytesToWrite);

			pLastBlock->setEndPos(m_blockSize);
			
			pLastBlock = acquireBlock();

			m_blocks.push_back(pLastBlock);
		}
		else
		{
			memcpy(pBlockData + endPos, pPos, bytesToWrite);
			
			pLastBlock->setEndPos(endPos + bytesToWrite);
		}

		numLeft -= bytesToWrite;
		m_bytesAvailable += bytesToWrite;
		pPos += bytesToWrite;
	}
}

int MIPStreamBuffer::read(void *pData, int amount)
{
	JMutexAutoLock autoLock(m_mutex);

	uint8_t *pPos = (uint8_t *)pData;
	int numLeft = amount;
	int numRead = 0;

	if (numLeft > m_bytesAvailable)
		numLeft = m_bytesAvailable;
	
	while (numLeft > 0)
	{
		BufferBlock *pBlock = *(m_blocks.begin());
		const uint8_t *pBlockData = pBlock->getData();
		int offset = pBlock->getOffset();
		int endPos = pBlock->getEndPos();
		int bytesInBlock = endPos - offset;
		int bytesToRead = numLeft;
		
		if (bytesInBlock < bytesToRead) // need to read more afterwards
		{
			bytesToRead = bytesInBlock;

			if (bytesToRead > 0)
				memcpy(pPos, pBlockData + offset, bytesToRead);

			releaseBlock(pBlock);
			m_blocks.pop_front();
		}
		else // everything we need to read is in this block
		{
			memcpy(pPos, pBlockData + offset, bytesToRead);

			pBlock->setOffset(offset + bytesToRead);
		}

		numLeft -= bytesToRead;
		m_bytesAvailable -= bytesToRead;
		pPos += bytesToRead;
		numRead += bytesToRead;
	}

	return numRead;
}

void MIPStreamBuffer::releaseBlock(BufferBlock *pBlock)
{
	if (m_numPreAllocAvailable >= m_numPreAlloc)
		delete pBlock;
	else
	{
		pBlock->setOffset(0);
		pBlock->setEndPos(0);
		
		m_preAllocatedBlocks[m_numPreAllocAvailable] = pBlock;
		m_numPreAllocAvailable++;
	}
}

MIPStreamBuffer::BufferBlock *MIPStreamBuffer::acquireBlock()
{
	if (m_numPreAllocAvailable == 0)
	{
		uint8_t *pData = new uint8_t[m_blockSize];
		return new BufferBlock(pData, 0);
	}

	m_numPreAllocAvailable--;
	BufferBlock *pBlock = m_preAllocatedBlocks[m_numPreAllocAvailable];
	return pBlock;
}

