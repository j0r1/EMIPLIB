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

#ifdef MIPCONFIG_SUPPORT_SNDFILE

#include "mipsndfileoutput.h"
#include "miprawaudiomessage.h"

#include "mipdebug.h"

#define MIPSNDFILEOUTPUT_ERRSTR_PULLUNSUPPORTED			"Pull is not supported"
#define MIPSNDFILEOUTPUT_ERRSTR_NOFILEOPENED			"No file was opened"
#define MIPSNDFILEOUTPUT_ERRSTR_FILEALREADYOPENED		"A file is already opened"
#define MIPSNDFILEOUTPUT_ERRSTR_BADMESSAGE			"Only raw audio messages are supported"
#define MIPSNDFILEOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Sampling rate in audio message is not the same as the sampling rate of the file"
#define MIPSNDFILEOUTPUT_ERRSTR_INCOMPATIBLECHANNELS		"The number of channels in the audio message is not the same as the number of channels of the file"
#define MIPSNDFILEOUTPUT_ERRSTR_ERRORWRITING			"Error writing to the file"
#define MIPSNDFILEOUTPUT_ERRSTR_UNSUPPORTEDBYTESPERSAMPLE	"Unsupported number of bytes per sample"
#define MIPSNDFILEOUTPUT_ERRSTR_CANTOPENFILE			"Can't open output file"
#define MIPSNDFILEOUTPUT_ERRSTR_CANTSEEKFILE			"Can't append to file"

MIPSndFileOutput::MIPSndFileOutput() : MIPComponent("MIPSndFileOutput")
{
	m_pSndFile = 0;
}

MIPSndFileOutput::~MIPSndFileOutput()
{
	close();
}

bool MIPSndFileOutput::open(const std::string &fname, int sampRate, int channels, int bytesPerSample, bool append)
{
	if (m_pSndFile != 0)
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_FILEALREADYOPENED);
		return false;
	}
	
	if (bytesPerSample < 1 || bytesPerSample > 4 || bytesPerSample == 3)
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_UNSUPPORTEDBYTESPERSAMPLE);
		return false;
	}
	
	SF_INFO inf;

	inf.frames = 0;
	inf.samplerate = sampRate;
	inf.channels = channels;
	inf.format = SF_FORMAT_WAV;

	switch (bytesPerSample)
	{
	case 2:
		inf.format |= SF_FORMAT_PCM_16;
		break;
	case 4:
		inf.format |= SF_FORMAT_PCM_32;
		break;
	case 1:
	default:
		inf.format |= SF_FORMAT_PCM_U8;
	}
	
	inf.sections = 0;
	inf.seekable = 0;

	if (append)
	{
		m_pSndFile = sf_open(fname.c_str(), SFM_RDWR, &inf);
		if (m_pSndFile == 0)
		{
			setErrorString(MIPSNDFILEOUTPUT_ERRSTR_CANTOPENFILE);
			return false;
		}
		if (sf_seek(m_pSndFile, 0, SEEK_END) == -1)
		{
			sf_close(m_pSndFile);
			m_pSndFile = 0;
			setErrorString(MIPSNDFILEOUTPUT_ERRSTR_CANTSEEKFILE);
			return false;
		}
	}
	else
	{
		m_pSndFile = sf_open(fname.c_str(), SFM_WRITE, &inf);
		if (m_pSndFile == 0)
		{
			setErrorString(MIPSNDFILEOUTPUT_ERRSTR_CANTOPENFILE);
			return false;
		}
	}
	
	m_channels = inf.channels;
	m_sampRate = inf.samplerate;
	
	return true;
}

bool MIPSndFileOutput::close()
{
	if (m_pSndFile == 0)
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_NOFILEOPENED);
		return false;
	}
	sf_close(m_pSndFile);
	m_pSndFile = 0;
	
	return true;
}

bool MIPSndFileOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pSndFile == 0)
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_NOFILEOPENED);
		return false;
	}

	MIPRawFloatAudioMessage *audioMessage = (MIPRawFloatAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}

	sf_count_t num = audioMessage->getNumberOfFrames();
	sf_count_t writeNum;
	
	writeNum = sf_writef_float(m_pSndFile, (float *)audioMessage->getFrames(), num);
	if (writeNum != num)
	{
		setErrorString(MIPSNDFILEOUTPUT_ERRSTR_ERRORWRITING);
		return false;
	}
	return true;
}

bool MIPSndFileOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPSNDFILEOUTPUT_ERRSTR_PULLUNSUPPORTED);
	return false;
}

#endif // MIPCONFIG_SUPPORT_SNDFILE
