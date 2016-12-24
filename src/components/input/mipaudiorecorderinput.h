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
 * \file mipaudiorecorderinput.h
 */

#ifndef MIPAUDIORECORDERINPUT_H

#define MIPAUDIORECORDERINPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_AUDIORECORDER

#include "mipcomponent.h"
#include "miptime.h"
#include "mipsignalwaiter.h"
#include <jthread/jthread.h>
#include <string>

class MIPRaw16bitAudioMessage;

namespace android
{
	class AudioRecord;
}

/** A component which uses the AudioRecord class available on the Android
 *  platform to capture audio.
 *  This component uses the AudioRecord class available on the Android
 *  platform to capture audio. It should be placed at the start of a chain
 *  and produces raw signed 16 bit audio messages, using native byte ordering.
 */
class EMIPLIB_IMPORTEXPORT MIPAudioRecorderInput : public MIPComponent, public jthread::JThread
{
public:
	MIPAudioRecorderInput();
	~MIPAudioRecorderInput();

	/** Opens the input component.
	 *  Opens the input component.
	 *  \param sampRate The sampling rate (e.g. 8000, 22050, 44100, ...)
	 *  \param channels The number of channels (e.g. 1 for mono, 2 for stereo)
	 *  \param interval This interval is used to capture audio.
	 *  \param arrayTime Size of the internal buffer used to temporarily store audio in, expressed as a 
	 *                   time interval. Note that this is not the amount of delay introduced by the component.
	 */
	bool open(int sampRate, int channels, MIPTime interval = MIPTime(0.020), MIPTime arrayTime = MIPTime(10.0));

	/** Closes the input component. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	static void StaticAudioRecorderCallback(int event, void *pUser, void *pInfo);
	void AudioRecorderCallback(void *pInfo);
	void *Thread();
	
	class RecordBuffer
	{
	public:
		RecordBuffer(size_t bufSize)						{ m_pBuffer = new uint16_t[bufSize]; }
		~RecordBuffer()								{ delete [] m_pBuffer; }
		uint16_t *getBuffer()							{ return m_pBuffer; }
		void setOffset(size_t o)						{ m_offset = o; }
		size_t getOffset() const						{ return m_offset; }
	private:
		uint16_t *m_pBuffer;
		size_t m_offset;
	};
	
	android::AudioRecord *m_pRecorder;

	int m_sampRate, m_channels;
	size_t m_blockSize, m_blockFrames;
	uint16_t *m_pLastInputCopy;
	uint16_t *m_pMsgBuffer;
	size_t m_bufSize;
	size_t m_frames;
	RecordBuffer **m_pRecordBuffers;
	int m_numBuffers;
	int m_curBuffer;
	int m_curReadBuffer;
	size_t m_availableFrames;
	MIPRaw16bitAudioMessage *m_pMsg;
	bool m_gotLastInput;
	bool m_gotMsg;
	bool m_stopThread;
	MIPSignalWaiter m_sigWait, m_sigWait2;
	jthread::JMutex m_frameMutex, m_blockMutex, m_stopMutex;
	MIPTime m_interval, m_sampleInstant;
};

#endif // MIPCONFIG_SUPPORT_AUDIORECORDER

#endif // MIPAUDIORECORDERINPUT_H

