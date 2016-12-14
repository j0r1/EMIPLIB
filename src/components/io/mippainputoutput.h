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

/**
 * \file mippainputoutput.h
 */

#ifndef MIPPAINPUTOUTPUT_H

#define MIPPAINPUTOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_PORTAUDIO

#include "mipcomponent.h"
#include "miptime.h"
#include "mipsignalwaiter.h"
#include <portaudio.h>
#include <jthread.h>
#include <string>

class MIPRaw16bitAudioMessage;
class MIPStreamBuffer;

/** A PortAudio input and output component.
 *  This component is a PortAudio soundcard input and output 
 *  component. The device  accepts two kinds of messages: MIPSYSTEMMESSAGE_WAITTIME 
 *  messages and 16 bit little endian raw audio messages (native endianness). The first
 *  message uses the input part of the component, the second type is sent to the 
 *  output part of the component. Messages produced by this component are 16 bit
 *  native endian raw audio messages.
 */
class MIPPAInputOutput : public MIPComponent
{
public:
	/** Indicates the access mode for the device. 
	 *  This is used to specify the access mode for the device. Use \c MIPPAInputOutput::ReadOnly
	 *  to only capture audio, \c MIPPAInputOutput::WriteOnly to only play back audio and 
	 *  \c MIPPAInputOutput::ReadWrite to do both. The device must support full duplex mode
	 *  to be opened in \c ReadWrite mode.
	 */
	enum AccessMode { ReadOnly, WriteOnly, ReadWrite };
	
	MIPPAInputOutput();
	~MIPPAInputOutput();
	
	/** Opens the soundcard device.
	 *  Using this function, a soundcard device is opened and initialized.
	 *  \param sampRate The sampling rate (e.g. 8000, 22050, 44100, ...)
	 *  \param channels The number of channels (e.g. 1 for mono, 2 for stereo)
	 *  \param interval This interval is used to capture audio and to play back audio.
	 *  \param accessMode Specifies the access mode for the device.
	 *  \param bufferTime Specifies how much memory is preallocated internally.
	 */
	bool open(int sampRate, int channels, MIPTime interval, AccessMode accessMode = WriteOnly, 
	          MIPTime bufferTime = MIPTime(10.0));
	
	/** Closes the device. */
	bool close();

	/** Initializes the PortAudio library and stores an error message if something
	 *  goes wrong. 
	 */
	static bool initializePortAudio(std::string &errorString);
	
	/** De-initializes the PortAudio library. */
	static void terminatePortAudio();
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	static int staticPACallback(const void *pInput, void *pOutput, unsigned long frameCount, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags statusFlags, void *pUserData);
	int portAudioCallback(const void *pInput, void *pOutput, unsigned long frameCount, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags statusFlags);
	
	PaStream *m_pStream;
	int m_sampRate;
	int m_channels;
	AccessMode m_accessMode;
	size_t m_blockFrames, m_blockBytes;
	MIPStreamBuffer *m_pOutputBuffer, *m_pInputBuffer;

	bool m_gotMsg;
	MIPSignalWaiter m_sigWait;
	MIPRaw16bitAudioMessage *m_pMsg;
	uint16_t *m_pMsgBuffer;
	bool m_gotLastInput;
};

#endif // MIPCONFIG_SUPPORT_PORTAUDIO

#endif // MIPPAINPUTOUTPUT_H

