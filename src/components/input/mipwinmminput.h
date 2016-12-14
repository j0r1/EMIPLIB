/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Hasselt University - Expertise Centre for
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
 * \file mipwinmminput.h
 */

#ifndef MIPWINMMINPUT_H

#define MIPWINMMINPUT_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include "mipsignalwaiter.h"
#include <jthread.h>
#include <windows.h>
#include <string>
#ifndef _WIN32_WCE
	#include <mmsystem.h>
#endif // _WIN32_WCE

class MIPRaw16bitAudioMessage;

/** Win32/WinCE Soundcard input component.
 *  This component allows sound to be captured on Win32 and WinCE platforms.
 *  It uses the MS-Windows Multimedia SDK waveIn... functions. This component
 *  should be used at the start of a chain: it accepts only MIPSYSTEMMESSAGE_WAITTIME
 *  messages. The messages generated are raw audio messages, containing 16 bit
 *  signed little endian data.
 */
class MIPWinMMInput : public MIPComponent, private JThread
{
public:
	MIPWinMMInput();
	~MIPWinMMInput();

	/** Open the sound capturing device.
	 *  This function opens the sound capturing device.
	 *  \param sampRate The sampling rate (e.g. 8000, 22050, 44100, ...)
	 *  \param channels The number of channels (e.g. 1 for mono, 2 for stereo)
	 *  \param interval The sampling interval.
	 *  \param bufferTime The component allocates a number of buffers to store
	 *         audio data in. This parameter specifies how much data these buffers
	 *         can contain, specified as a time interval.
	 *  \param highPriority If \c true, the thread of the chain in which this
	 *                      component resides and a background thread in which
	 *                      the sampling takes place will both receive the
	 *                      highest priority.
	 */
	bool open(int sampRate, int channels, MIPTime interval = MIPTime(0.020), 
	          MIPTime bufferTime = MIPTime(10.0), bool highPriority = false);

	/** Closes the sound capturing device.
	 *  This function closes a previously opened sound capturing device.
	 */
	bool close();
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void *Thread();
	static void CALLBACK inputCallback(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

	bool m_init;
	HWAVEIN m_device;
	int m_sampRate;
	int m_channels;
	uint16_t *m_pLastInputCopy;
	uint16_t *m_pMsgBuffer;
	MIPRaw16bitAudioMessage *m_pMsg;
	size_t m_bufSize,m_frames;
	bool m_gotLastInput;
	bool m_gotMsg;
	bool m_stopThread;
	WAVEHDR *m_inputBuffers;
	int m_numBuffers;
	MIPSignalWaiter m_sigWait,m_threadSigWait;
	JMutex m_frameMutex,m_stopMutex;
	MIPTime m_interval, m_sampleInstant;
	bool m_threadPrioritySet;

	std::string m_threadError;
};

#endif // MIPWINMMINPUT_H

