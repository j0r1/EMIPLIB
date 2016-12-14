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

#ifdef MIPCONFIG_SUPPORT_JACK

#include "mipjackoutput.h"
#include "miprawaudiomessage.h"
#include <iostream>

#include "mipdebug.h"

#define MIPJACKOUTPUT_ERRSTR_CLIENTALREADYOPEN		"Already opened a client"
#define MIPJACKOUTPUT_ERRSTR_CANTOPENCLIENT		"Error opening client"
#define MIPJACKOUTPUT_ERRSTR_PULLNOTIMPLEMENTED		"Pull not implemented"
#define MIPJACKOUTPUT_ERRSTR_CANTREGISTERCHANNEL	"Unable to register a new port"
#define MIPJACKOUTPUT_ERRSTR_ARRAYTIMETOOSMALL		"Array time too small"
#define MIPJACKOUTPUT_ERRSTR_CLIENTNOTOPEN		"Client is not running"
#define MIPJACKOUTPUT_ERRSTR_BADMESSAGE			"Not a floating point raw audio message"
#define MIPJACKOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE	"Incompatible sampling rate"
#define MIPJACKOUTPUT_ERRSTR_NOTSTEREO			"Not stereo audio"
#define MIPJACKOUTPUT_ERRSTR_CANTGETPHYSICALPORTS	"Unable to obtain physical ports"
#define MIPJACKOUTPUT_ERRSTR_NOTENOUGHPORTS		"Less than two ports were found, not enough for stereo"
#define MIPJACKOUTPUT_ERRSTR_CANTCONNECTPORTS		"Unable to connect ports"
#define MIPJACKOUTPUT_ERRSTR_CANTACTIVATECLIENT		"Can't activate client"

MIPJackOutput::MIPJackOutput() : MIPComponent("MIPJackOutput"), m_delay(0), m_distTime(0), m_blockTime(0), m_interval(0)
{
	int status;
	
	m_pClient = 0;
	
	if ((status = m_frameMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize Jack callback mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPJackOutput::~MIPJackOutput()
{
	close();
}

bool MIPJackOutput::open(MIPTime interval, const std::string &serverName, MIPTime arrayTime)
{
	if (m_pClient != 0)
	{
		setErrorString(MIPJACKOUTPUT_ERRSTR_CLIENTALREADYOPEN);
		return false;
	}
	
	if (arrayTime.getValue() < 2.0) // Should be at least two since one second will be filled in 
	{                               // in the close function
		setErrorString(MIPJACKOUTPUT_ERRSTR_ARRAYTIMETOOSMALL);
		return false;
	}
	
	std::string clientName("MIPJackOutput");
	const char *pServName = 0;
	jack_options_t options = JackNullOption;
	jack_status_t jackStatus;
	
	if (serverName.length() != 0)
	{
		pServName = serverName.c_str();
		options = (jack_options_t)((int)options|(int)JackServerName);
	}

	m_pClient = jack_client_open(clientName.c_str(), options, &jackStatus, pServName);
	if (m_pClient == 0)
	{
		setErrorString(MIPJACKOUTPUT_ERRSTR_CANTOPENCLIENT);
		return false;
	}
	
	jack_set_process_callback (m_pClient, StaticJackCallback, this);

	m_pLeftOutput = jack_port_register(m_pClient, "leftChannel", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	m_pRightOutput = jack_port_register(m_pClient, "rightChannel", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if (m_pLeftOutput == 0 || m_pRightOutput == 0)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		setErrorString(MIPJACKOUTPUT_ERRSTR_CANTREGISTERCHANNEL);
		return false;
	}

	m_sampRate = jack_get_sample_rate(m_pClient);
	m_blockLength = jack_get_buffer_size(m_pClient);
	m_blockTime = MIPTime((real_t)m_blockLength/(real_t)m_sampRate);
	m_frameArrayLength = ((size_t)((arrayTime.getValue() * ((real_t)m_sampRate)) + 0.5));
	m_intervalLength = ((((size_t)((interval.getValue()*(real_t)m_sampRate)+0.5))/m_blockLength)+1)*m_blockLength;
	m_interval = MIPTime((real_t)m_intervalLength/(real_t)m_sampRate);

	if (m_intervalLength > m_frameArrayLength/4)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		setErrorString(MIPJACKOUTPUT_ERRSTR_ARRAYTIMETOOSMALL);
		return false;
	}

	// make sure a fixed number of blocks fit in the frame array
	size_t numBlocks = m_frameArrayLength / m_blockLength + 1;
	m_frameArrayLength = numBlocks * m_blockLength;
	
	m_pFrameArrayLeft = new float[m_frameArrayLength];
	m_pFrameArrayRight = new float[m_frameArrayLength];
	
	for (size_t i = 0 ; i < m_frameArrayLength ; i++)
	{
		m_pFrameArrayLeft[i] = 0;
		m_pFrameArrayRight[i] = 0;
	}
	
	m_currentPos = 0;
	m_nextPos = m_intervalLength;
	m_distTime = m_interval;

	if (jack_activate(m_pClient) != 0)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		delete [] m_pFrameArrayRight;
		delete [] m_pFrameArrayLeft;
		setErrorString(MIPJACKOUTPUT_ERRSTR_CANTACTIVATECLIENT);
		return false;
	}

	const char **pPhysPorts = jack_get_ports(m_pClient, 0, 0, JackPortIsPhysical|JackPortIsInput);
	if (pPhysPorts == 0)
	{
		jack_client_close(m_pClient);
		m_pClient = 0;
		delete [] m_pFrameArrayRight;
		delete [] m_pFrameArrayLeft;
		setErrorString(MIPJACKOUTPUT_ERRSTR_CANTGETPHYSICALPORTS);
		return false;
	}
	
	for (int i = 0 ; i < 2 ; i++)
	{
		if (pPhysPorts[i] == 0)
		{
			free(pPhysPorts);
			jack_client_close(m_pClient);
			m_pClient = 0;
			delete [] m_pFrameArrayRight;
			delete [] m_pFrameArrayLeft;
			setErrorString(MIPJACKOUTPUT_ERRSTR_NOTENOUGHPORTS);
			return false;
		}

		jack_port_t *pSrc = 0;
		
		if (i == 0)
			pSrc = m_pLeftOutput;
		else
			pSrc = m_pRightOutput;
			
		if (jack_connect(m_pClient, jack_port_name(pSrc), pPhysPorts[i]) != 0)
		{
			free(pPhysPorts);
			jack_client_close(m_pClient);
			m_pClient = 0;
			delete [] m_pFrameArrayRight;
			delete [] m_pFrameArrayLeft;
			setErrorString(MIPJACKOUTPUT_ERRSTR_CANTCONNECTPORTS);
			return false;
		}
	}
			
	free(pPhysPorts);

	return true;
}

bool MIPJackOutput::close()
{
	if (m_pClient == 0)
	{
		setErrorString(MIPJACKOUTPUT_ERRSTR_CLIENTNOTOPEN);
		return false;
	}

	// Fill the buffer with a second of silence
	
	size_t silenceSize = m_sampRate*2;
	float *frames = new float[silenceSize];
	size_t num = m_sampRate;
	size_t offset = 0;
	
	memset(frames, 0, silenceSize*sizeof(float));
	
	m_frameMutex.Lock();
	while (num > 0)
	{
		size_t maxAmount = m_frameArrayLength - m_nextPos;
		size_t amount = (num > maxAmount)?maxAmount:num;
		
		if (m_nextPos <= m_currentPos && m_nextPos + amount > m_currentPos)
		{
			//std::cerr << "Strange error" << std::endl;
			m_nextPos = m_currentPos + m_blockLength;
		}
		
		if (amount != 0)
		{
			size_t i;
			float *pDestLeft = m_pFrameArrayLeft + m_nextPos;
			float *pDestRight = m_pFrameArrayRight + m_nextPos;
			const float *pSrc = frames + offset;
			
			for (i = 0 ;  i < amount ; i++, pDestRight++, pDestLeft++, pSrc += 2)
			{
				pDestLeft[0] = pSrc[0];
				pDestRight[0] = pSrc[1];
			}
		}
		
		if (amount != num)
		{
			m_nextPos = 0;
//			std::cerr << "Cycling next pos" << std::endl;
		}
		else
			m_nextPos += amount;
		
		offset += amount;
		num -= amount;
	}
	
	m_frameMutex.Unlock();

	// Now we wait a short while
	
	MIPTime::wait(MIPTime(0,200000));
	delete [] frames;

	// And we can close the device (now we can be pretty sure that no nasty block will be looping)
	
	jack_client_close(m_pClient);
	m_pClient = 0;

	delete [] m_pFrameArrayRight;
	delete [] m_pFrameArrayLeft;

	return true;
}

bool MIPJackOutput::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && 
		pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT ))
	{
		setErrorString(MIPJACKOUTPUT_ERRSTR_BADMESSAGE);
		return false;
	}
	
	if (m_pClient == 0)
	{
		setErrorString(MIPJACKOUTPUT_ERRSTR_CLIENTNOTOPEN);
		return false;
	}
	
	MIPAudioMessage *audioMessage = (MIPAudioMessage *)pMsg;
	
	if (audioMessage->getSamplingRate() != m_sampRate)
	{
		setErrorString(MIPJACKOUTPUT_ERRSTR_INCOMPATIBLESAMPLINGRATE);
		return false;
	}
	if (audioMessage->getNumberOfChannels() != 2)
	{
		setErrorString(MIPJACKOUTPUT_ERRSTR_NOTSTEREO);
		return false;
	}
	
	size_t num = audioMessage->getNumberOfFrames();
	size_t offset = 0;

	MIPRawFloatAudioMessage *audioMessageFloat = (MIPRawFloatAudioMessage *)pMsg;
	const float *frames = audioMessageFloat->getFrames();

	m_frameMutex.Lock();
	while (num > 0)
	{
		size_t maxAmount = m_frameArrayLength - m_nextPos;
		size_t amount = (num > maxAmount)?maxAmount:num;
		
		if (m_nextPos <= m_currentPos && m_nextPos + amount > m_currentPos)
		{
			//std::cerr << "Strange error" << std::endl;
			m_nextPos = m_currentPos + m_blockLength;
		}
		
		if (amount != 0)
		{
			size_t i;
			float *pDestLeft = m_pFrameArrayLeft + m_nextPos;
			float *pDestRight = m_pFrameArrayRight + m_nextPos;
			const float *pSrc = frames + offset;
			
			for (i = 0 ;  i < amount ; i++, pDestRight++, pDestLeft++, pSrc += 2)
			{
				pDestLeft[0] = pSrc[0];
				pDestRight[0] = pSrc[1];
			}
		}
		
		if (amount != num)
		{
			m_nextPos = 0;
//			std::cerr << "Cycling next pos" << std::endl;
		}
		else
			m_nextPos += amount;
		
		offset += amount;
		num -= amount;
	}
	
	m_distTime += MIPTime(((real_t)audioMessage->getNumberOfFrames())/((real_t)m_sampRate));
	m_frameMutex.Unlock();

	return true;
}

bool MIPJackOutput::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString(MIPJACKOUTPUT_ERRSTR_PULLNOTIMPLEMENTED);
	return false;
}

int MIPJackOutput::StaticJackCallback(jack_nframes_t nframes, void *arg)
{
	MIPJackOutput *pInst = (MIPJackOutput *)arg;
	return pInst->JackCallback(nframes);
}

int MIPJackOutput::JackCallback(jack_nframes_t nframes)
{
	if (nframes != m_blockLength)
	{
//		std::cout << "Block length changed!" << std::endl;
		return -1;
	}
	
	void *pLeftBuf = jack_port_get_buffer(m_pLeftOutput, nframes);
	void *pRightBuf = jack_port_get_buffer(m_pRightOutput, nframes);

	memcpy(pLeftBuf, m_pFrameArrayLeft + m_currentPos, nframes*sizeof (jack_default_audio_sample_t));
	memcpy(pRightBuf, m_pFrameArrayRight + m_currentPos, nframes*sizeof (jack_default_audio_sample_t));
	
	m_frameMutex.Lock();
	m_currentPos += m_blockLength;
	m_distTime -= m_blockTime;
	
	if (m_currentPos + m_blockLength > m_frameArrayLength)
	{
		m_currentPos = 0;
//		std::cerr << "Cycling" << std::endl;
	}
		
	if (m_nextPos < m_currentPos + m_blockLength && m_nextPos >= m_currentPos)
	{
		m_nextPos = (m_currentPos + m_intervalLength) % m_frameArrayLength;
		m_distTime = m_interval;
//		std::cerr << "Pushing next position" << std::endl;
	}
		
	if (m_distTime > MIPTime(0.200))
	{
		m_nextPos = (m_currentPos + m_intervalLength) % m_frameArrayLength;
		m_distTime = m_interval;
//		std::cerr << "Adjusting to runaway input (" << m_distTime.getString() << ")" << std::endl;
	}
	m_frameMutex.Unlock();		

	return 0;
}

#endif // MIPCONFIG_SUPPORT_JACK

