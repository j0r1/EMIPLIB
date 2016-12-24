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

/**
 * \file mipjackinput.h
 */

#ifndef MIPJACKINPUT_H

#define MIPJACKINPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_JACK

#include "mipcomponent.h"
#include "miptime.h"
#include "mipsignalwaiter.h"
#include <jthread/jthread.h>
#include <string>
#include <jack/jack.h>

class MIPRawFloatAudioMessage;

/** A Jack audio input component.
 *  This component can be used to capture audio using the Jack audio connection
 *  kit. The component only accepts MIPSYSTEMMESSAGE_TYPE_WAITTIME messages and
 *  it produces floating point stereo raw audio messages.
 *  \note A frame consists of a number of samples (equal to the number of channels)
 *        sampled at the same instant. For example, in a stereo sound file, one
 *        frame would consist of two samples: one for the left channel and one for
 *        the right.
 */
class EMIPLIB_IMPORTEXPORT MIPJackInput : public MIPComponent, public jthread::JThread
{
public:
	MIPJackInput();
	~MIPJackInput();

	/** Opens the connection with the Jack server.
	 *  This function initializes the component.
	 *  \param interval The amount of audio frames read during each iteration, expressed as a time interval
	 *  \param serverName The name of the Jack server to connect to
	 *  \param arrayTime Size of the internal buffer used to temporarily store audio in, expressed as a 
	 *                   time interval. Note that this is not the amount of delay introduced by the component.
	 */
	bool open(MIPTime interval = MIPTime(0.020), const std::string &serverName = std::string(""), MIPTime arrayTime = MIPTime(10.0));

	/** Returns the sampling rate used for outgoing messages. */
	int getSamplingRate() const									{ if (m_pClient == 0) return 0; return m_sampRate; }

	/** Closes the connection with the Jack server. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	static int StaticJackCallback(jack_nframes_t nframes, void *arg);
	int JackCallback(jack_nframes_t nframes);
	void *Thread();
	
	class RecordBuffer
	{
	public:
		RecordBuffer(size_t bufSize)						{ m_pLeftBuffer = new jack_default_audio_sample_t [bufSize]; m_pRightBuffer = new jack_default_audio_sample_t [bufSize]; }
		~RecordBuffer()								{ delete [] m_pLeftBuffer; delete [] m_pRightBuffer; }
		jack_default_audio_sample_t *getLeftBuffer()				{ return m_pLeftBuffer; }
		jack_default_audio_sample_t *getRightBuffer()				{ return m_pRightBuffer; }
		void setOffset(size_t o)						{ m_offset = o; }
		size_t getOffset() const						{ return m_offset; }
	private:
		jack_default_audio_sample_t *m_pLeftBuffer, *m_pRightBuffer;
		size_t m_offset;
	};
	
	jack_client_t *m_pClient;
	jack_port_t *m_pLeftInput;
	jack_port_t *m_pRightInput;
	
	int m_sampRate;
	size_t m_blockSize;
	float *m_pLastInputCopy;
	float *m_pMsgBuffer;
	size_t m_bufSize;
	size_t m_frames;
	RecordBuffer **m_pRecordBuffers;
	int m_numBuffers;
	int m_curBuffer;
	int m_curReadBuffer;
	size_t m_availableFrames;
	MIPRawFloatAudioMessage *m_pMsg;
	bool m_gotLastInput;
	bool m_gotMsg;
	bool m_stopThread;
	MIPSignalWaiter m_sigWait, m_sigWait2;
	jthread::JMutex m_frameMutex, m_blockMutex, m_stopMutex;
	MIPTime m_interval, m_sampleInstant;
};

#endif // MIPCONFIG_SUPPORT_JACK

#endif // MIPJACKINPUT_H

