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

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_AUDIOFILE

#include "mipaudiofileinput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"

#include "mipdebug.h"

#define MIPAUDIOFILEINPUT_ERRSTR_NOFILEOPENED			"No file opened"
#define MIPAUDIOFILEINPUT_ERRSTR_FILEALREADYOPENED		"Already opened a file"
#define MIPAUDIOFILEINPUT_ERRSTR_BADMESSAGE			"Message is not a timing event"
#define MIPAUDIOFILEINPUT_ERRSTR_CANTOPENFILE			"Can't open file"
#define MIPAUDIOFILEINPUT_ERRSTR_NOTSUPPORTED			"File is not supported by the AF library"
#define MIPAUDIOFILEINPUT_ERRSTR_SAMPLEFORMATUNKNOWN		"Sample format unknown"
#define MIPAUDIOFILEINPUT_ERRSTR_BYTEORDERUNKNOWN		"Byte order unknown"
#define	MIPAUDIOFILEINPUT_ERRSTR_NOTIMPLEMENTED			"Not implemented"
#define	MIPAUDIOFILEINPUT_ERRSTR_OUTOFMEMORY			"Out of memory"
#define MIPAUDIOFILEINPUT_ERRSTR_CANTSETVIRTUALSAMPLEFORMAT	"Unable to set output to 16 bit signed samples"

MIPAudioFileInput::MIPAudioFileInput() : MIPComponent("MIPAudioFileInput")
{
	m_audiofileHandle = 0;
}

MIPAudioFileInput::~MIPAudioFileInput()
{
	close();
}

bool MIPAudioFileInput::open(const std::string &fname, int frames, bool loop)
{
	if (m_audiofileHandle != 0)
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}

	m_audiofileHandle = afOpenFile(fname.c_str(),"r",NULL);
	if (m_audiofileHandle == 0)
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_CANTOPENFILE);
		return false;
	}

	m_sampRate = (int)(afGetRate(m_audiofileHandle, AF_DEFAULT_TRACK));
	m_numChannels = afGetChannels(m_audiofileHandle, AF_DEFAULT_TRACK);

	if (afSetVirtualSampleFormat(m_audiofileHandle, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16) != 0)
	{
		afCloseFile(m_audiofileHandle);
		m_audiofileHandle = 0;
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_CANTSETVIRTUALSAMPLEFORMAT);
		return false;
	}
	
	int byteOrder = afGetVirtualByteOrder(m_audiofileHandle, AF_DEFAULT_TRACK);	
	
	switch(byteOrder) 
	{
	case AF_BYTEORDER_BIGENDIAN:
		m_sampleEncoding = MIPRaw16bitAudioMessage::BigEndian;
		break;
	case AF_BYTEORDER_LITTLEENDIAN:
		m_sampleEncoding = MIPRaw16bitAudioMessage::LittleEndian;
		break;
	default:
		afCloseFile(m_audiofileHandle);
		m_audiofileHandle = 0;
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_BYTEORDERUNKNOWN);
		return false;
	}

	m_numFrames = frames;
	m_loop = loop;	
	m_pFrames = new uint16_t[m_numFrames*m_numChannels];
	m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_numChannels, m_numFrames, true, m_sampleEncoding, m_pFrames, false);
	m_eof = false;
	m_gotMessage = false;
	
	m_sourceID = 0;

	return true;
}

bool MIPAudioFileInput::open(const std::string &fname, MIPTime interval, bool loop)
{
	if (m_audiofileHandle != 0)
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}

	m_audiofileHandle = afOpenFile(fname.c_str(),"r",NULL);
	if (m_audiofileHandle == 0)
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_CANTOPENFILE);
		return false;
	}

	m_sampRate = (int)(afGetRate(m_audiofileHandle, AF_DEFAULT_TRACK));
	m_numChannels = afGetChannels(m_audiofileHandle, AF_DEFAULT_TRACK);

	if (afSetVirtualSampleFormat(m_audiofileHandle, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16) != 0)
	{
		afCloseFile(m_audiofileHandle);
		m_audiofileHandle = 0;
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_CANTSETVIRTUALSAMPLEFORMAT);
		return false;
	}
	
	int byteOrder = afGetVirtualByteOrder(m_audiofileHandle, AF_DEFAULT_TRACK);	
	
	switch(byteOrder) 
	{
	case AF_BYTEORDER_BIGENDIAN:
		m_sampleEncoding = MIPRaw16bitAudioMessage::BigEndian;
		break;
	case AF_BYTEORDER_LITTLEENDIAN:
		m_sampleEncoding = MIPRaw16bitAudioMessage::LittleEndian;
		break;
	default:
		afCloseFile(m_audiofileHandle);
		m_audiofileHandle = 0;
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_BYTEORDERUNKNOWN);
		return false;
	}

	int frames = (int)(interval.getValue()*((real_t)m_sampRate)+0.5);
	
	m_numFrames = frames;
	m_loop = loop;	
	m_pFrames = new uint16_t[m_numFrames*m_numChannels];
	m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_numChannels, m_numFrames, true, m_sampleEncoding, m_pFrames, false);
	m_eof = false;
	m_gotMessage = false;
	
	m_sourceID = 0;

	return true;
}

bool MIPAudioFileInput::close()
{
	if (m_audiofileHandle == 0)
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	afCloseFile(m_audiofileHandle);
	m_audiofileHandle = 0;
	
	delete [] m_pFrames;
	delete m_pMsg;
	
	return true;
}

bool MIPAudioFileInput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_ISTIME))
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_audiofileHandle == 0)
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	if (!m_eof) // if mEof is true, we've already filled up the array with 0's
	{
		int num, readNum;
	
		num = m_numFrames;
		readNum = afReadFrames(m_audiofileHandle, AF_DEFAULT_TRACK, m_pFrames, num);
	
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
				afSeekFrame(m_audiofileHandle, AF_DEFAULT_TRACK, 0);
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

bool MIPAudioFileInput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_audiofileHandle == 0)
	{
		setErrorString(MIPAUDIOFILEINPUT_ERRSTR_NOFILEOPENED);
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

#endif // MIPCONFIG_SUPPORT_AUDIOFILE

