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

#ifdef MIPCONFIG_SUPPORT_ESD

#include "mipesdoutput.h"
#include "miprawaudiomessage.h"
#include <unistd.h>

#define MIPESDOUTPUT_ERRSTR_DEVICEALREADYOPEN		"Device already opened"
#define MIPESDOUTPUT_ERRSTR_DEVICENOTOPEN		"Device is not opened"
#define MIPESDOUTPUT_ERRSTR_CANTOPENDEVICE		"Error opening device"
#define MIPESDOUTPUT_ERRSTR_CANTSETINTERLEAVEDMODE	"Can't set device to interleaved mode"
#define MIPESDOUTPUT_ERRSTR_FLOATNOTSUPPORTED		"Floating point format is not supported by this device"
#define MIPESDOUTPUT_ERRSTR_UNSUPPORTEDSAMPLINGRATE	"The requested sampling rate is not supported"
#define MIPESDOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS		"The requested number of channels is not supported"
#define MIPESDOUTPUT_ERRSTR_BLOCKTIMETOOLARGE		"Block time too large"
#define MIPESDOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"No pull available for this component"
#define MIPESDOUTPUT_ERRSTR_BADMESSAGE			"Only raw audio messages are supported"
#define MIPESDOUTPUT_ERRSTR_INCOMPATIBLECHANNELS	"Incompatible number of channels"
#define MIPESDOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPESDOUTPUT_ERRSTR_BUFFERFULL			"Buffer full"
#define MIPESDOUTPUT_ERRSTR_UNKNOWNCHANNELS		"ESD only supports mono or stereo sound, using more then two channels is not supported"
#define MIPESDOUTPUT_ERRSTR_UNSUPPORTEDBITS		"ESD only supports 8 and 16 bit samples"
#define MIPESDOUTPUT_ERRSTR_PROBLEMWRITING		"Problem writing to ESD"

MIPEsdOutput::MIPEsdOutput() : MIPComponent("MIPEsdOutput"), m_opened(false)
{
}

MIPEsdOutput::~MIPEsdOutput()
{
	close();
}

bool MIPEsdOutput::open(int sampRate, int channels, int sampWidth, MIPTime blockTime, MIPTime arrayTime)
{
	if (m_opened)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_DEVICEALREADYOPEN);
		return false;
	}
	
	esd_format_t esd_fmt = 0;
	
	m_channels = channels;
	switch(m_channels)
	{
		case 1:
			esd_fmt |= ESD_MONO;
			break;
		case 2:
			esd_fmt |= ESD_STEREO;
			break;
		default:
			setErrorString(MIPESDOUTPUT_ERRSTR_UNKNOWNCHANNELS);
			return false;
	}

	m_sampWidth = sampWidth;
	switch(m_sampWidth)
	{
		case 8:
			esd_fmt |= ESD_BITS8;
			break;
		case 16:
			esd_fmt |= ESD_BITS16;
			break;
		default:
			setErrorString(MIPESDOUTPUT_ERRSTR_UNSUPPORTEDBITS);
			return false;
	}

	m_sampRate = sampRate;
	m_esd_handle = esd_play_stream(esd_fmt, m_sampRate, "localhost", "emipesd");
	if (m_esd_handle < 0)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}
	m_opened = true;
	return true;
}

bool MIPEsdOutput::close()
{
	if (!m_opened)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	esd_audio_close();
	::close(m_esd_handle);
		
	return true;
}

bool MIPEsdOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_opened)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW))
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_U8)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_UNSUPPORTEDBITS);
		return false;
	}

	MIPRaw16bitAudioMessage *audioMessage = (MIPRaw16bitAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}

	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPESDOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}
	
	size_t amount = audioMessage->getNumberOfFrames() * m_channels;
	const uint16_t *frames = audioMessage->getFrames();

	if (amount != 0)
	{
		if (write(m_esd_handle, frames, amount*sizeof(uint16_t)) < 0)
		{
			setErrorString(MIPESDOUTPUT_ERRSTR_PROBLEMWRITING);
			return false;
		}

	}

	return true;
}

bool MIPEsdOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPESDOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return false;
}
#endif // MIPCONFIG_SUPPORT_ESD

