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

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_PULSEAUDIO

#include "mippulseoutput.h"
#include "miprawaudiomessage.h"
#include <stdlib.h>
#include <iostream>

#include "mipdebug.h"

#define MIPPULSEOUTPUT_ERRSTR_CLIENTALREADYOPEN		"Already opened a client"
#define MIPPULSEOUTPUT_ERRSTR_CANTOPENCLIENT		"Error opening client"
#define MIPPULSEOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"Pull not implemented"
#define MIPPULSEOUTPUT_ERRSTR_CANTREGISTERCHANNEL	"Unable to register a new port"
#define MIPPULSEOUTPUT_ERRSTR_ARRAYTIMETOOSMALL		"Array time too small"
#define MIPPULSEOUTPUT_ERRSTR_CLIENTNOTOPEN		"Client is not running"
#define MIPPULSEOUTPUT_ERRSTR_BADMESSAGE			"Not a floating point raw audio message"
#define MIPPULSEOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPPULSEOUTPUT_ERRSTR_NOTSTEREO			"Not stereo audio"
#define MIPPULSEOUTPUT_ERRSTR_CANTGETPHYSICALPORTS	"Unable to obtain physical ports"
#define MIPPULSEOUTPUT_ERRSTR_NOTENOUGHPORTS		"Less than two ports were found, not enough for stereo"
#define MIPPULSEOUTPUT_ERRSTR_CANTCONNECTPORTS		"Unable to connect ports"
#define MIPPULSEOUTPUT_ERRSTR_CANTACTIVATECLIENT		"Can't activate client"
#define MIPPULSEOUTPUT_ERRSTR_BADSAMPLINGRATE "Invalid sampling rate specified, must be a positive number of maximum 48000"
#define MIPPULSEOUTPUT_ERRSTR_BADCHANNELS "Invalid number of channels specified, must be one or two"

MIPPulseOutput::MIPPulseOutput() : MIPComponent("MIPPulseOutput")
{
	int status;
	
	m_pStream = 0;
}

MIPPulseOutput::~MIPPulseOutput()
{
	close();
}

bool MIPPulseOutput::open(int samplingRate, int channels, MIPTime interval, const std::string &serverName)
{
	if (m_pStream != 0)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_CLIENTALREADYOPEN);
		return false;
	}

	if (samplingRate < 0 || samplingRate > 48000)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_BADSAMPLINGRATE);
		return false;
	}

	if (channels < 1 || channels > 2)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_BADCHANNELS);
		return false;
	}
	
	std::string clientName("MIPPulseOutput");
	const char *pServName = 0;
	const char *pDeviceName = 0; // TODO
	
	if (serverName.length() != 0)
		pServName = serverName.c_str();

	pa_sample_spec sampleSpec;

	sampleSpec.rate = (uint32_t)samplingRate;
	sampleSpec.channels = (uint8_t)channels;
#ifdef MIPCONFIG_BIGENDIAN
	sampleSpec.format = PA_SAMPLE_FLOAT32BE;
#else
	sampleSpec.format = PA_SAMPLE_FLOAT32LE;
#endif

	// TODO: use async api to get a low-latency version?

	m_pStream = pa_simple_new(pServName, "MIPPulseOutput", PA_STREAM_PLAYBACK,
	                          pDeviceName, "MIPPulseOutput output stream", &sampleSpec,
							  nullptr, // channel map?
							  nullptr, // buffer attributes?
							  nullptr // precise error code
	                          );

	if (m_pStream == 0)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_CANTOPENCLIENT);
		return false;
	}
	
	m_sampRate = samplingRate;
	m_channels = channels;

	return true;
}

bool MIPPulseOutput::close()
{
	if (m_pStream == 0)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_CLIENTNOTOPEN);
		return false;
	}

	pa_simple_free(m_pStream);
	m_pStream = 0;

	return true;
}

bool MIPPulseOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
		pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT ))
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pStream == 0)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_CLIENTNOTOPEN);
		return false;
	}
	
	MIPAudioMessage *audioMessage = (MIPAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPPULSEOUTPUT_ERRSTR_NOTSTEREO);
		return false;
	}
	
	size_t num = audioMessage->getNumberOfFrames();

	MIPRawFloatAudioMessage *audioMessageFloat = (MIPRawFloatAudioMessage *)pMsg;
	const float *pFrames = audioMessageFloat->getFrames();

	int errCode = 0;
	uint64_t latencyUSec = pa_simple_get_latency(m_pStream, &errCode);
	//if (errCode == 0)
	//	std::cout << "latencyUSec = " << latencyUSec << std::endl;
	
	// write output
	pa_simple_write(m_pStream, pFrames, sizeof(float)*num*m_channels, nullptr);

	return true;
}

bool MIPPulseOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPPULSEOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return false;
}


#endif // MIPCONFIG_SUPPORT_PULSEAUDIO

