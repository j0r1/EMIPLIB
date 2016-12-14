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

#ifdef MIPCONFIG_SUPPORT_PORTAUDIO

#include "mippainputoutput.h"
#include "miprawaudiomessage.h"
#include "mipsystemmessage.h"
#include "mipstreambuffer.h"
#include <iostream>

#include "mipdebug.h"

#define MIPPAINPUTOUTPUT_ERRSTR_ALREADYOPENED			"A device has already been opened"
#define MIPPAINPUTOUTPUT_ERRSTR_BUFFERTOOSMALL			"The specified buffer size is too small"
#define MIPPAINPUTOUTPUT_ERRSTR_CANTOPEN			"Unable to open the device: "
#define MIPPAINPUTOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS		"Only mono or stereo sound is supported"
#define MIPPAINPUTOUTPUT_ERRSTR_NOTOPENED			"No device has been opened yet"
#define MIPPAINPUTOUTPUT_ERRSTR_BADMESSAGE			"An invalid message was received"
#define MIPPAINPUTOUTPUT_ERRSTR_DEVICENOTOPENEDFORREADING	"Pull not supported because the device wasn't opened for reading"
#define MIPPAINPUTOUTPUT_ERRSTR_CANTSTART			"Unable to start the stream: "
#define MIPPAINPUTOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPPAINPUTOUTPUT_ERRSTR_INCOMPATIBLECHANNELS		"Incompatible sampling number of channels"
#define MIPPAINPUTOUTPUT_ERRSTR_INCOMPATIBLEFRAMES		"Incompatible sampling number of frames"
#define MIPPAINPUTOUTPUT_ERRSTR_CANTINITSIGWAIT			"Can't initialize the signal waiter"

MIPPAInputOutput::MIPPAInputOutput() : MIPComponent("MIPPAInputOutput")
{
	m_pStream = 0;
}

MIPPAInputOutput::~MIPPAInputOutput()
{
	close();
}

bool MIPPAInputOutput::open(int sampRate, int channels, MIPTime interval, MIPPAInputOutput::AccessMode accessMode, 
                            MIPTime bufferTime)
{
	if (m_pStream != 0)
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_ALREADYOPENED);
		return false;
	}
	
	if (!(channels == 1 || channels == 2))
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_UNSUPPORTEDCHANNELS);
		return false;
	}

	int inputChannels = 0;
	int outputChannels = 0;
	
	if (accessMode == ReadOnly)
		inputChannels = channels;
	else if (accessMode == WriteOnly)
		outputChannels = channels;
	else
	{
		inputChannels = channels;
		outputChannels = channels;
	}
	
	m_sampRate = sampRate;
	m_channels = channels;
	m_accessMode = accessMode;

	// prepare buffers

	size_t bufferLength = ((size_t)(bufferTime.getValue() * ((real_t)m_sampRate) + 0.5)) * channels;
	m_blockFrames = (size_t)(interval.getValue()*((real_t)m_sampRate) + 0.5);
	size_t blockLength = m_blockFrames*channels;
	m_blockBytes = blockLength*sizeof(int16_t);
	
	if (blockLength > bufferLength/4.0)
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_BUFFERTOOSMALL);
		return false;
	}
	
	size_t num = (size_t)(((real_t)bufferLength)/((real_t)blockLength)+0.5);

	m_pInputBuffer = 0;
	m_pOutputBuffer = 0;
	
	if (accessMode == ReadOnly || accessMode == ReadWrite)
	{
		if (!m_sigWait.init())
		{
			setErrorString(MIPPAINPUTOUTPUT_ERRSTR_CANTINITSIGWAIT);
			return false;
		}
		m_pMsgBuffer = new uint16_t [blockLength];
		for (size_t i = 0 ; i < blockLength ; i++)
			m_pMsgBuffer[i] = 0;
		
		m_gotLastInput = false;
		m_pMsg = new MIPRaw16bitAudioMessage(m_sampRate, m_channels, m_blockFrames, true, MIPRaw16bitAudioMessage::Native, m_pMsgBuffer, false);
	}
	else
	{
		m_pMsgBuffer = 0;
		m_pMsg = 0;
	}
	
	if (accessMode == ReadOnly || accessMode == ReadWrite)
		m_pInputBuffer = new MIPStreamBuffer(m_blockBytes, num);
	if (accessMode == WriteOnly || accessMode == ReadWrite)
		m_pOutputBuffer = new MIPStreamBuffer(m_blockBytes, num);
	
	// Initialize stream
	
	PaError err;

	err = Pa_OpenDefaultStream(&m_pStream, inputChannels, outputChannels, paInt16, m_sampRate, m_blockFrames, staticPACallback, this);
	if (err != paNoError)
	{
		m_pStream = 0;
	
		if (m_pInputBuffer)
			delete m_pInputBuffer;
		if (m_pOutputBuffer)
			delete m_pOutputBuffer;
		m_sigWait.destroy();
		if (m_pMsg)
			delete m_pMsg;
		if (m_pMsgBuffer)
			delete m_pMsgBuffer;
			
		setErrorString(std::string(MIPPAINPUTOUTPUT_ERRSTR_CANTOPEN) + std::string(Pa_GetErrorText(err)));
		return false;
	}

	err = Pa_StartStream(m_pStream);
	if (err != paNoError)
	{
		m_pStream = 0;
	
		if (m_pInputBuffer)
			delete m_pInputBuffer;
		if (m_pOutputBuffer)
			delete m_pOutputBuffer;
		m_sigWait.destroy();
		if (m_pMsg)
			delete m_pMsg;
		if (m_pMsgBuffer)
			delete m_pMsgBuffer;
				
		setErrorString(std::string(MIPPAINPUTOUTPUT_ERRSTR_CANTSTART) + std::string(Pa_GetErrorText(err)));
		return false;
	}
			
	m_gotMsg = false;

	return true;
}

bool MIPPAInputOutput::close()
{
	if (m_pStream == 0)
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}

	Pa_StopStream(m_pStream);
	Pa_CloseStream(m_pStream);

	m_pStream = 0;
	
	if (m_pInputBuffer)
		delete m_pInputBuffer;
	if (m_pOutputBuffer)
		delete m_pOutputBuffer;
	m_sigWait.destroy();
	if (m_pMsg)
		delete m_pMsg;
	if (m_pMsgBuffer)
		delete m_pMsgBuffer;
	
	return true;
}

bool MIPPAInputOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (m_pStream == 0)
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}
	
	if ((m_accessMode == ReadOnly || m_accessMode == ReadWrite) && 
	    pMsg->getMessageType() == MIPMESSAGE_TYPE_SYSTEM && 
	    pMsg->getMessageSubtype() == MIPSYSTEMMESSAGE_TYPE_WAITTIME)
	{
		/*m_sigWait.clearSignalBuffers();
		
		if (m_pInputBuffer->getAmountBuffered() < m_blockBytes)
		*/	m_sigWait.waitForSignal();
		/*
		while (m_pInputBuffer->getAmountBuffered() >= m_blockBytes) // make sure we got the most recent block
		*/	m_pInputBuffer->read(m_pMsgBuffer, m_blockBytes);

		m_gotMsg = false;
	}
	else if ((m_accessMode == WriteOnly || m_accessMode == ReadWrite) && 
	         pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW &&
		 pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_S16)
	{
		MIPRaw16bitAudioMessage *pAudioMsg = (MIPRaw16bitAudioMessage *)pMsg;

		if (m_channels != pAudioMsg->getNumberOfChannels())
		{
			setErrorString(MIPPAINPUTOUTPUT_ERRSTR_INCOMPATIBLECHANNELS);
			return false;
		}
		if (m_blockFrames != pAudioMsg->getNumberOfFrames())
		{
			setErrorString(MIPPAINPUTOUTPUT_ERRSTR_INCOMPATIBLEFRAMES);
			return false;
		}
		if (m_sampRate != pAudioMsg->getSamplingRate())
		{
			setErrorString(MIPPAINPUTOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
			return false;
		}

		if (m_pOutputBuffer->getAmountBuffered() > 5*m_blockBytes) // TODO: make this configurable
			m_pOutputBuffer->clear();

		m_pOutputBuffer->write(pAudioMsg->getFrames(), m_blockBytes);
	}
	else
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	return true;
}

bool MIPPAInputOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (m_pStream == 0)
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_NOTOPENED);
		return false;
	}

	if (!(m_accessMode == ReadOnly || m_accessMode == ReadWrite))
	{
		setErrorString(MIPPAINPUTOUTPUT_ERRSTR_DEVICENOTOPENEDFORREADING);
		return false;
	}

	if (m_gotMsg)
	{
		m_gotMsg = false;
		*pMsg = 0;
	}
	else
	{
		m_gotMsg = true;
		*pMsg = m_pMsg;
	}
	return true;
}

int MIPPAInputOutput::staticPACallback(const void *pInput, void *pOutput, unsigned long frameCount, 
                                       const PaStreamCallbackTimeInfo *pTimeInfo, 
				       PaStreamCallbackFlags statusFlags, void *pUserData)
{
	MIPPAInputOutput *pInstance = (MIPPAInputOutput *)pUserData;

	return pInstance->portAudioCallback(pInput, pOutput, frameCount, pTimeInfo, statusFlags);
}

int MIPPAInputOutput::portAudioCallback(const void *pInput, void *pOutput, unsigned long frameCount, 
                                        const PaStreamCallbackTimeInfo *pTimeInfo, 
					PaStreamCallbackFlags statusFlags)
{
	int numBytesRead = m_pOutputBuffer->read(pOutput, m_blockBytes);

	if (numBytesRead < m_blockBytes)
		memset(((uint8_t *)pOutput)+numBytesRead, 0, m_blockBytes-numBytesRead);

	m_pInputBuffer->write(pInput, m_blockBytes);
	m_sigWait.signal();

	return 0;
}

bool MIPPAInputOutput::initializePortAudio(std::string &errorString)
{
	PaError err;

	err = Pa_Initialize();
	if (err != paNoError)
	{
		errorString = std::string(Pa_GetErrorText(err));
		return false;
	}
	return true;
}

void MIPPAInputOutput::terminatePortAudio()
{
	Pa_Terminate();
}
	
#endif // MIPCONFIG_SUPPORT_PORTAUDIO

