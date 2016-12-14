/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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
 * \file mipsignalwaiter.h
 */

#ifndef MIPSIGNALWAITER_H

#define MIPSIGNALWAITER_H

#include "mipconfig.h"
#include <jmutex.h>

#define MIPSIGNALWAITER_DUMMYBUFLEN					1

/** Objects of this type can be used to send signals between threads.
 *  This kind of object can be used to send signals between threads. For
 *  example, soundcard input plugins use such an object to indicate when
 *  a new block of sound data is available.
 */
class MIPSignalWaiter
{
public:
	MIPSignalWaiter();
	~MIPSignalWaiter();
	
	/** Initialized the object. */
	bool init();

	/** Destroys a previously initialized signalling object. */
	void destroy();

	/** Waits until somewhere else a signal is generated. */
	bool waitForSignal();

	/** Generates a signal. */
	bool signal();

	/**  Clears the signalling buffers; this returns the object to its state right after initialization. */
	bool clearSignalBuffers();

	/** Returns \c true if the object is already initialized, \c false otherwise. */
	bool isInit()								{ return m_init; }
private:
#ifndef WIN32
	int m_sigPipe[2];
#else
	HANDLE m_eventObject;
#endif // WIN32
	bool m_init;
	char m_dummyBuf[MIPSIGNALWAITER_DUMMYBUFLEN];
	JMutex m_countMutex,m_waitMutex;
	int m_count;
	bool m_isWaiting;
};

#endif // MIPSIGNALWAITER_H

