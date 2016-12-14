/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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
#include "mipwavoutput.h"
#include "miprawaudiomessage.h"
#include "mipwavwriter.h"

#include "mipdebug.h"

#define MIPWAVOUTPUT_ERRSTR_PULLUNSUPPORTED			"Pull is not supported"
#define MIPWAVOUTPUT_ERRSTR_NOFILEOPENED			"No file was opened"
#define MIPWAVOUTPUT_ERRSTR_FILEALREADYOPENED		"A file is already opened"
#define MIPWAVOUTPUT_ERRSTR_BADMESSAGE			"Only raw audio messages (8 bit, unsigned) are supported"
#define MIPWAVOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Sampling rate in audio message is not the same as the sampling rate of the file"
#define MIPWAVOUTPUT_ERRSTR_INCOMPATIBLECHANNELS		"The number of channels in the audio message is not equal to one"
#define MIPWAVOUTPUT_ERRSTR_ERRORWRITING			"Error writing to the file"
#define MIPWAVOUTPUT_ERRSTR_CANTOPENFILE			"Can't open output file"
#define MIPWAVOUTPUT_ERRSTR_CANTSEEKFILE			"Can't append to file"

MIPWAVOutput::MIPWAVOutput() : MIPComponent("MIPWAVOutput")
{
	m_pSndFile = 0;
}

MIPWAVOutput::~MIPWAVOutput()
{
	close();
}

bool MIPWAVOutput::open(const std::string &fname, int sampRate)
{
	if (m_pSndFile != 0)
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}
	
	m_pSndFile = new MIPWAVWriter();
	if (!m_pSndFile->open(fname, sampRate))
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_CANTOPENFILE);
		delete m_pSndFile;
		m_pSndFile = 0;
		return false;
	}
	
	m_sampRate = sampRate;
	
	return true;
}

bool MIPWAVOutput::close()
{
	if (m_pSndFile == 0)
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	delete m_pSndFile;
	m_pSndFile = 0;
	
	return true;
}

bool MIPWAVOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_U8))
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pSndFile == 0)
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	MIPRawU8AudioMessage *audioMessage = (MIPRawU8AudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != 1)
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}

	int num = audioMessage->getNumberOfFrames();
	
	if (!m_pSndFile->writeFrames(audioMessage->getFrames(), num))
	{
		setErrorString(MIPWAVOUTPUT_ERRSTR_ERRORWRITING);
		return false;
	}
	return true;
}

bool MIPWAVOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPWAVOUTPUT_ERRSTR_PULLUNSUPPORTED);
	return false;
}

