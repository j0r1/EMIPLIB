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
 * \file mipossinputoutput.h
 */

#ifndef MIPOSSINPUTOUTPUT_H

#define MIPOSSINPUTOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_OSS

#include "mipcomponent.h"
#include "miptime.h"
#include "mipsignalwaiter.h"
#include <jthread/jthread.h>
#include <string>

class MIPRaw16bitAudioMessage;

/** Parameters for a MIPOSSInputOutput instance. */
class EMIPLIB_IMPORTEXPORT MIPOSSInputOutputParams
{
public:
	MIPOSSInputOutputParams() : m_bufferTime(10.0), m_ossBufferTime(0.007), m_devName("/dev/dsp")
											{ m_exactRate = true; m_ossFragments = 6; }
	~MIPOSSInputOutputParams()							{ }

	/** Sets a flag indicating if only the exact specified sampling rate can be used (default: \c true). */
	void setUseExactRate(bool f)							{ m_exactRate = f; }

	/** Sets a parameter specifying the amount of memory which is allocated for output buffers.
	 *  Using this function, the amount of memory which is allocated for output buffers can be
	 *  set. Note that this is definitely not the amount of buffering which is introduced. The
	 *  default value is 10.0 seconds.
	 */
	void setBufferTime(MIPTime t)							{ m_bufferTime = t; }

	/** Sets the name of the device to use (default: \c /dev/dsp). */ 
	void setDeviceName(const std::string &name)					{ m_devName = name; }

	/** Sets the size of internal buffer fragments for the OSS driver.
	 *  Using this function, you can change the size of a buffer fragment used
	 *  by the OSS driver. The default value is 0.007 seconds.
	 */
	void setOSSBufferSize(MIPTime t)						{ m_ossBufferTime = t; }

	/** Sets the amount of buffer fragments for the OSS driver.
	 *  Using this function, the number of buffer fragments used by the OSS driver
	 *  can be changed. The default number of fragments is 6.
	 */
	void setOSSFragments(uint16_t n)						{ m_ossFragments = n; }

	/** Returns true if the exact specified sampling rate should be used. */
	bool useExactRate() const							{ return m_exactRate; }

	/** Returns the amount of memory which is allocated for output buffers. */
	MIPTime getBufferTime() const							{ return m_bufferTime; }

	/** Returns the name of the device which will be used. */
	std::string getDeviceName() const						{ return m_devName; }

	/** Returns the size of the buffer fragments which will be used by the OSS driver. */
	MIPTime getOSSBufferSize() const						{ return m_ossBufferTime; }

	/** Returns the number of buffer fragments which will be used by the OSS driver. */
	uint16_t getOSSFragments() const						{ return m_ossFragments; }
private:
	bool m_exactRate;
	uint16_t m_ossFragments;
	MIPTime m_bufferTime, m_ossBufferTime;
	std::string m_devName;
};

/** An Open Sound System (OSS) input and output component.
 *  This component is an Open Sound System (OSS) soundcard input and output 
 *  component. The device  accepts two kinds of messages: MIPSYSTEMMESSAGE_WAITTIME 
 *  messages and 16 bit raw audio messages. The first type of 
 *  message uses the input part of the component, the second type is sent to the 
 *  output part of the component. Messages produced by this component are 16 bit
 *  raw audio messages. A trivial echo chain would have 
 *  an object of this type as start component and a connection from this object to 
 *  this object, thereby immediately playing back the captured audio.
 */
class EMIPLIB_IMPORTEXPORT MIPOSSInputOutput : public MIPComponent
{
public:
	/** Indicates the access mode for the device. 
	 *  This is used to specify the access mode for the device. Use \c MIPOSSInputOutput::ReadOnly
	 *  to only capture audio, \c MIPOSSInputOutput::WriteOnly to only play back audio and 
	 *  \c MIPOSSInputOutput::ReadWrite to do both. The device must support full duplex mode
	 *  to be opened in \c ReadWrite mode.
	 */
	enum AccessMode { ReadOnly, WriteOnly, ReadWrite };
	
	MIPOSSInputOutput();
	~MIPOSSInputOutput();
	
	/** Opens the soundcard device.
	 *  Using this function, a soundcard device is opened and initialized.
	 *  \param sampRate The sampling rate (e.g. 8000, 22050, 44100, ...)
	 *  \param channels The number of channels (e.g. 1 for mono, 2 for stereo)
	 *  \param interval This interval is used to capture audio and to play back audio.
	 *  \param accessMode Specifies the access mode for the device.
	 *  \param pParams Additional parameters. If set to null, default values will be used.
	 */
	bool open(int sampRate, int channels, MIPTime interval, AccessMode accessMode = WriteOnly, 
	          const MIPOSSInputOutputParams *pParams = 0);
	
	/** Returns the sampling rate which is used. */
	int getSamplingRate() const							{ return m_sampRate; }

	/** Returns the raw audio subtype which is used by the component. */
	uint32_t getRawAudioSubtype() const						{ return m_audioSubtype; }
	
	/** Closes the device. */
	bool close();
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	class IOThread : public jthread::JThread
	{
	public:
		IOThread(MIPOSSInputOutput &ossIO);
		~IOThread();
		void stop();
	protected:
		MIPOSSInputOutput &m_ossIO;
		jthread::JMutex m_stopMutex;
		bool m_stopLoop;
	};

	class InputThread : public IOThread
	{
	public:
		InputThread(MIPOSSInputOutput &ossIO) : IOThread(ossIO)		{ }
		~InputThread()							{ stop(); }
		void *Thread();
	};
	
	class OutputThread : public IOThread
	{
	public:
		OutputThread(MIPOSSInputOutput &ossIO) : IOThread(ossIO)	{ }
		~OutputThread()							{ stop(); }
		void *Thread();
	};

	bool inputThreadStep();
	bool outputThreadStep();

	InputThread *m_pInputThread;
	OutputThread *m_pOutputThread;

	friend class InputThread;
	friend class OutputThread;

	int m_device;
	int m_sampRate;
	int m_channels;
	AccessMode m_accessMode;
	uint32_t m_audioSubtype;
	bool m_isSigned;

	uint16_t *m_pInputBuffer;
	uint16_t *m_pLastInputCopy;
	uint16_t *m_pMsgBuffer;
	MIPRaw16bitAudioMessage *m_pMsg;
	bool m_gotLastInput;
	bool m_gotMsg;
	MIPSignalWaiter m_sigWait;
	MIPTime m_sampleInstant;

	uint16_t *m_pOutputBuffer;
	size_t m_bufferLength, m_blockLength, m_blockFrames;
	size_t m_curOutputPos;
	size_t m_nextOutputPos;
	MIPTime m_blockTime,m_outputDistTime;

	jthread::JMutex m_inputFrameMutex, m_outputFrameMutex;
};

#endif // MIPCONFIG_SUPPORT_OSS

#endif // MIPOSSINPUTOUTPUT_H

