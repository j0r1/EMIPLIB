/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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

#ifdef MIPCONFIG_SUPPORT_SNDFILE

#include "mipsndfileinput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"

#include "mipdebug.h"

#define MIPSNDFILEINPUT_ERRSTR_NOFILEOPENED			"No file opened"
#define MIPSNDFILEINPUT_ERRSTR_FILEALREADYOPENED		"Already opened a file"
#define MIPSNDFILEINPUT_ERRSTR_BADMESSAGE			"Message is not a timing event"
#define MIPSNDFILEINPUT_ERRSTR_CANTOPENFILE			"Can't open file"

MIPSndFileInput::MIPSndFileInput() : MIPComponent("MIPSndFileInput")
{
	m_pSndFile = 0;
}

MIPSndFileInput::~MIPSndFileInput()
{
	close();
}

bool MIPSndFileInput::open(const std::string &fname, int frames, bool loop)
{
	if (m_pSndFile != 0)
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}

	SF_INFO inf;

	inf.frames = 0;
	inf.samplerate = 0;
	inf.channels = 0;
	inf.format = 0;
	inf.sections = 0;
	inf.seekable = 0;
	
	m_pSndFile = sf_open(fname.c_str(), SFM_READ, &inf);
	if (m_pSndFile == 0)
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_CANTOPENFILE);
		return false;
	}

	m_sampRate = inf.samplerate;
	m_numFrames = frames;
	m_numChannels = inf.channels;
	m_loop = loop;	
	m_pFrames = new float[m_numFrames*m_numChannels];
	m_pMsg = new MIPRawFloatAudioMessage(m_sampRate, m_numChannels, m_numFrames, m_pFrames, false);
	m_eof = false;
	m_gotMessage = false;
	
	m_sourceID = 0;
	
	return true;
}

bool MIPSndFileInput::open(const std::string &fname, MIPTime interval, bool loop)
{
	if (m_pSndFile != 0)
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}

	SF_INFO inf;

	inf.frames = 0;
	inf.samplerate = 0;
	inf.channels = 0;
	inf.format = 0;
	inf.sections = 0;
	inf.seekable = 0;
	
	m_pSndFile = sf_open(fname.c_str(), SFM_READ, &inf);
	if (m_pSndFile == 0)
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_CANTOPENFILE);
		return false;
	}

	m_sampRate = inf.samplerate;
	m_numChannels = inf.channels;

	int frames = (int)(interval.getValue()*((real_t)m_sampRate)+0.5);
	
	m_numFrames = frames;
	m_loop = loop;	
	m_pFrames = new float[m_numFrames*m_numChannels];
	m_pMsg = new MIPRawFloatAudioMessage(m_sampRate, m_numChannels, m_numFrames, m_pFrames, false);
	m_eof = false;
	m_gotMessage = false;
	
	m_sourceID = 0;

	return true;
}

bool MIPSndFileInput::close()
{
	if (m_pSndFile == 0)
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	sf_close(m_pSndFile);
	m_pSndFile = 0;
	
	delete [] m_pFrames;
	delete m_pMsg;
	
	return true;
}

bool MIPSndFileInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_ISTIME))
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pSndFile == 0)
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	if (!m_eof) // if mEof is true, we've already filled up the array with 0's
	{
		sf_count_t num, readNum;
	
		num = m_numFrames;
		readNum = sf_readf_float(m_pSndFile, m_pFrames, num);
	
		if (readNum < num)
		{
			int arrayLen = m_numFrames*m_numChannels;
			int startPos, i;
	
			if (readNum <= 0)
				startPos = 0;
			else
				startPos = readNum*m_numChannels;
				
			for (i = startPos ; i < arrayLen ; i++)
				m_pFrames[i] = 0;	
	
			if (m_loop)
				sf_seek(m_pSndFile, 0, SEEK_SET);
			else
			{
				if (readNum <= 0)
					m_eof = true;
			}
		}
	}
	m_pMsg->setSourceID(m_sourceID);
	m_gotMessage = false;
	
	return true;
}

bool MIPSndFileInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pSndFile == 0)
	{
		setErrorString(MIPSNDFILEINPUT_ERRSTR_NOFILEOPENED);
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

#endif // MIPCONFIG_SUPPORT_SNDFILE

