/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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

#ifdef MIPCONFIG_SUPPORT_OPENAL

#include "mipopenaloutput.h"
#include "miprawaudiomessage.h"
#include <cassert>
#include <iostream>

using std::map;

#define MIPOPENALOUTPUT_ERRSTR_ALREADYOPENED             "A device was already opened"
#define MIPOPENALOUTPUT_ERRSTR_CANTOPENDEVICE            "Can't open device"
#define MIPOPENALOUTPUT_ERRSTR_DEVICENOTOPEN             "Device is not opened"
#define MIPOPENALOUTPUT_ERRSTR_CANTCREATEBUFFER          "Can't create OpenAL buffer"
#define MIPOPENALOUTPUT_ERRSTR_CANTCREATESOURCE          "Can't create OpenAL source"
#define MIPOPENALOUTPUT_ERRSTR_BADMESSAGE                "Only raw audio messages are supported"
#define MIPOPENALOUTPUT_ERRSTR_INCOMPATIBLECHANNELS      "Incompatible number of channels"
#define MIPOPENALOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE  "Incompatible sampling rate"
#define MIPOPENALOUTPUT_ERRSTR_PULLNOTIMPLEMENTED        "No pull available for this component"
#define MIPOPENALOUTPUT_ERRSTR_COULDNTFILLBUFFER         "Unable to fill OpenAL buffer with audio data"
#define MIPOPENALOUTPUT_ERRSTR_INVALIDSOURCE             "Invalid source position"
#define MIPOPENALOUTPUT_ERRSTR_INVALIDLISTENER           "Invalid own position"

MIPOpenALOutput::MIPOpenALOutput() : MIPComponent("MIPOpenALOutput")
{
	m_init = false;
}

MIPOpenALOutput::~MIPOpenALOutput()
{
	close();
}

bool MIPOpenALOutput::open(int sampRate, int channels, int precision)
{
	if (m_init)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_ALREADYOPENED);
		return false;
	}

	m_sampRate = sampRate;
	m_channels = channels;
	m_format = calcFormat(m_channels, precision);

	// Open the default device
	m_pDevice = alcOpenDevice(NULL);
	if (!m_pDevice)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_CANTOPENDEVICE);
		return false;
	}

	// Open a context on that device
	m_pContext = alcCreateContext(m_pDevice, NULL);

	m_init = true;
	return true;
}

bool MIPOpenALOutput::close()
{
	if (!m_init)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	ALuint source;
    ALuint buffer;
	int numQueued;

	alcMakeContextCurrent(m_pContext);
	map<uint64_t, ALuint>::iterator srcIt = m_sources.begin();
	for (; srcIt != m_sources.end(); srcIt++)
	{
		source = (*srcIt).second;
		alSourceStop(source);

		alGetSourcei(source, AL_BUFFERS_QUEUED, &numQueued);
		while(numQueued--)
		{
			alSourceUnqueueBuffers(source, 1, &buffer);
			alDeleteBuffers(1, &buffer);
		}

		alDeleteSources(1, &source);
	}

	alcMakeContextCurrent(NULL);
	alcDestroyContext(m_pContext);
	alcCloseDevice(m_pDevice);

	m_init = false;

	return true;
}

bool MIPOpenALOutput::setSourcePosition(uint64_t sourceID, real_t pos[3])
{
	if (!m_init)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	ALuint source;

	// Retrieve the OpenAL source
	if (!getSource(sourceID, &source))
		return false;

	// Make the stored context current
	alcMakeContextCurrent(m_pContext);
	assert(alGetError() == AL_NO_ERROR);

	// Set source position
	alSource3f(source, AL_POSITION, pos[0], pos[1], pos[2]);

	if (alGetError() != AL_NO_ERROR)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_INVALIDSOURCE);
		return false;
	}

	return true;
}

bool MIPOpenALOutput::setOwnPosition(real_t pos[3], real_t frontDirection[3], real_t upDirection[3])
{
	if (!m_init)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	ALfloat orientation[6];
	orientation[0] = frontDirection[0];
	orientation[1] = frontDirection[1];
	orientation[2] = frontDirection[2];
	orientation[3] = upDirection[0];
	orientation[4] = upDirection[1];
	orientation[5] = upDirection[2];

	// Make the stored context current
	alcMakeContextCurrent(m_pContext);
	assert(alGetError() == AL_NO_ERROR);

	alListener3f(AL_POSITION, pos[0], pos[1], pos[2]);
	alListenerfv(AL_ORIENTATION, orientation);

	if (alGetError() != AL_NO_ERROR)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_INVALIDLISTENER);
		return false;
	}

	return true;
}

bool MIPOpenALOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_DEVICENOTOPEN);
		return false;
	}

	ALuint source;
	MIPRaw16bitAudioMessage *pAudioMessage;

	// Verify message type
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16))
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	pAudioMessage = (MIPRaw16bitAudioMessage *)pMsg;

	// Retrieve the associated OpenAL source
	if (!getSource(pAudioMessage->getSourceID(), &source))
		return false;

	// Check sampling rate and number of channels
	if (pAudioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (pAudioMessage->getNumberOfChannels() != m_channels)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
		return false;
	}

	// Enqueue the raw audio data to the source
	size_t num = pAudioMessage->getNumberOfFrames() * m_channels;
	const uint16_t *pFrames = pAudioMessage->getFrames();

	enqueueData(source, (void *)pFrames, num * sizeof(uint16_t));

	return true;
}

bool MIPOpenALOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPOPENALOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return true;
}

bool MIPOpenALOutput::getSource(uint64_t sourceID, ALuint *pSource)
{
	map<uint64_t, ALuint>::const_iterator it = m_sources.find(sourceID);

	if (it != m_sources.end())
	{
		*pSource = (*it).second;
	}
	else
	{
		// Make the stored context current
		alcMakeContextCurrent(m_pContext);
		assert(alGetError() == AL_NO_ERROR);

		// Generate an OpenAL audio source
		alGenSources(1, pSource);
		if (alGetError() != AL_NO_ERROR)
		{
			setErrorString(MIPOPENALOUTPUT_ERRSTR_CANTCREATESOURCE);
			return false;
		}

		// Store the source
		m_sources[sourceID] = *pSource;
	}

	return true;
}

bool MIPOpenALOutput::enqueueData(ALuint source, void *pData, size_t size)
{
	ALuint buffer;
	ALint sourceState;
	ALint numProcessed;

	// Make the stored context current
	alcMakeContextCurrent(m_pContext);
	assert(alGetError() == AL_NO_ERROR);

	// Retrieve the number of buffers available
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &numProcessed);
	assert(alGetError() == AL_NO_ERROR);

	// If no previously attached buffers are already processed and ready to reuse, generate a new one
	if (numProcessed == 0)
	{
		alGenBuffers(1, &buffer);
		if (alGetError() != AL_NO_ERROR)
		{
			setErrorString(MIPOPENALOUTPUT_ERRSTR_CANTCREATEBUFFER);
			return false;
		}
	}
	// Otherwise, retrieve one of the already processed buffers
	else
	{
		// Retrieve an already processed buffer
		alSourceUnqueueBuffers(source, 1, &buffer);
		assert(alGetError() == AL_NO_ERROR);
	}

	// Fill the buffer with data
	alBufferData(buffer, m_format, pData, size, m_sampRate);
	if (alGetError() != AL_NO_ERROR)
	{
		setErrorString(MIPOPENALOUTPUT_ERRSTR_COULDNTFILLBUFFER);
		return false;
	}

	// Attach the buffer to the source
	alSourceQueueBuffers(source, 1, &buffer);
	assert(alGetError() == AL_NO_ERROR);

	// Make sure the source is playing
	alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
	if (sourceState != AL_PLAYING)
		alSourcePlay(source);

	return true;
}

ALenum MIPOpenALOutput::calcFormat(int channels, int precision)
{
	if (precision == 8) // 8-bit encoding not supported at the moment
		return (channels == 1)? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
	return (channels == 1)? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
}

#endif // MIPCONFIG_SUPPORT_OPENAL

