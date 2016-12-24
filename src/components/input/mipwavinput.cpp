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
#include "mipwavinput.h"
#include "mipwavreader.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"

#include "mipdebug.h"

#define MIPWAVINPUT_ERRSTR_NOFILEOPENED			"No file opened"
#define MIPWAVINPUT_ERRSTR_FILEALREADYOPENED		"Already opened a file"
#define MIPWAVINPUT_ERRSTR_BADMESSAGE			"Message is not a timing event"
#define MIPWAVINPUT_ERRSTR_CANTOPENFILE			"Can't open file: "

MIPWAVInput::MIPWAVInput() : MIPComponent("MIPWAVInput")
{
	m_pSndFile = 0;
}

MIPWAVInput::~MIPWAVInput()
{
	close();
}

bool MIPWAVInput::open(const std::string &fname, int frames, bool loop, bool intSamples)
{
	if (m_pSndFile != 0)
	{
		setErrorString(MIPWAVINPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}

	m_pSndFile = new MIPWAVReader();
	if (!m_pSndFile->open(fname))
	{
		setErrorString(std::string(MIPWAVINPUT_ERRSTR_CANTOPENFILE) + m_pSndFile->getErrorString());
		delete m_pSndFile;
		m_pSndFile = 0;
		return false;
	}

	m_sampRate = m_pSndFile->getSamplingRate();
	m_numFrames = frames;
	m_numChannels = m_pSndFile->getNumberOfChannels();
	m_loop = loop;	
	if (intSamples)
	{
		m_pFramesInt = new uint16_t[m_numFrames*m_numChannels];
		m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_numChannels, m_numFrames, true, MIPRaw16bitAudioMessage::Native, m_pFramesInt, false);
	}
	else
	{
		m_pFramesFloat = new float[m_numFrames*m_numChannels];
		m_pMsg = new MIPRawFloatAudioMessage(m_sampRate, m_numChannels, m_numFrames, m_pFramesFloat, false);
	}
	m_eof = false;
	m_gotMessage = false;
	m_intSamples = intSamples;
	
	m_sourceID = 0;
	
	return true;
}

bool MIPWAVInput::open(const std::string &fname, MIPTime interval, bool loop, bool intSamples)
{
	if (m_pSndFile != 0)
	{
		setErrorString(MIPWAVINPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}

	m_pSndFile = new MIPWAVReader();
	if (!m_pSndFile->open(fname))
	{
		setErrorString(std::string(MIPWAVINPUT_ERRSTR_CANTOPENFILE) + m_pSndFile->getErrorString());
		delete m_pSndFile;
		m_pSndFile = 0;
		return false;
	}
	m_sampRate = m_pSndFile->getSamplingRate();
	m_numChannels = m_pSndFile->getNumberOfChannels();

	int frames = (int)(interval.getValue()*((real_t)m_sampRate)+0.5);
	
	m_numFrames = frames;
	m_loop = loop;	
	if (intSamples)
	{
		m_pFramesInt = new uint16_t[m_numFrames*m_numChannels];
		m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_numChannels, m_numFrames, true, MIPRaw16bitAudioMessage::Native, m_pFramesInt, false);
	}
	else
	{
		m_pFramesFloat = new float[m_numFrames*m_numChannels];
		m_pMsg = new MIPRawFloatAudioMessage(m_sampRate, m_numChannels, m_numFrames, m_pFramesFloat, false);
	}
	m_eof = false;
	m_gotMessage = false;
	m_intSamples = intSamples;
	
	m_sourceID = 0;
	
	return true;
}

bool MIPWAVInput::close()
{
	if (m_pSndFile == 0)
	{
		setErrorString(MIPWAVINPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	delete m_pSndFile;
	m_pSndFile = 0;
	
	if (m_intSamples)
		delete [] m_pFramesInt;
	else
		delete [] m_pFramesFloat;
	delete m_pMsg;
	
	return true;
}

bool MIPWAVInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_ISTIME))
	{
		setErrorString(MIPWAVINPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pSndFile == 0)
	{
		setErrorString(MIPWAVINPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	if (!m_eof) // if mEof is true, we've already filled up the array with 0's
	{
		int num, readNum;
	
		num = m_numFrames;
		if (m_intSamples)
			m_pSndFile->readFrames((int16_t *)m_pFramesInt, num, &readNum);
		else
			m_pSndFile->readFrames(m_pFramesFloat, num, &readNum);
	
		if (readNum < num)
		{
			int arrayLen = m_numFrames*m_numChannels;
			int startPos, i;
	
			if (readNum <= 0)
				startPos = 0;
			else
				startPos = readNum*m_numChannels;
			
			if (m_intSamples)
			{
				for (i = startPos ; i < arrayLen ; i++)
					m_pFramesInt[i] = 0;	
			}
			else
			{
				for (i = startPos ; i < arrayLen ; i++)
					m_pFramesFloat[i] = 0;	
			}
			
			if (m_loop)
				m_pSndFile->rewind();
			else
			{
				if (readNum <= 0)
					m_eof = true;
			}

			onLastInputFrame(); // inform that we're at the last block
		}
	}
	m_pMsg->setSourceID(m_sourceID);
	m_gotMessage = false;
	
	return true;
}

bool MIPWAVInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pSndFile == 0)
	{
		setErrorString(MIPWAVINPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	if (!m_gotMessage)
	{
		m_gotMessage = true;
		*pMsg = m_pMsg;
	}
	else
	{
		m_gotMessage = false;
		*pMsg = 0;
	}
	
	return true;
}

