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
#include "mipsignalwaiter.h"
#ifdef WIN32
	#include <string.h>
#else // unix like functions
	#include <sys/types.h>
	#include <sys/time.h>
	#include <sys/ioctl.h>
	#include <unistd.h>
#endif // WIN32
#include <iostream>

#include "mipdebug.h"

MIPSignalWaiter::MIPSignalWaiter()
{
	m_init = false;

	int status;

	if ((status = m_countMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize signal waiter count mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
	
	if ((status = m_waitMutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize signal waiter wait mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}
}

MIPSignalWaiter::~MIPSignalWaiter()
{
}

void MIPSignalWaiter::destroy()
{
	if (!m_init)
		return;
#ifndef WIN32
	pthread_cond_destroy(&m_cond);
	pthread_mutex_destroy(&m_mutex);
#else
	CloseHandle(m_eventObject);
#endif // WIN32
	m_init = false;
}

bool MIPSignalWaiter::init()
{
	if (m_init)
	{
		// TODO error description
		return false;
	}
#ifdef WIN32
	if ((m_eventObject = CreateEvent(NULL, true, false, NULL)) == NULL)
	{
		// TODO error description
		return false;
	}
#else // unix like function
	if (pthread_cond_init(&m_cond, 0) < 0)
	{
		// TODO error description
		return false;
	}
	if (pthread_mutex_init(&m_mutex, 0) < 0)
	{
		// TODO error description
		pthread_cond_destroy(&m_cond);
		return false;
	}
#endif // WIN32
	m_init = true;
	m_isWaiting = false;
	m_count = 0;
	return true;
}

bool MIPSignalWaiter::waitForSignal()
{
	if (!m_init)
	{
		// TODO error description
		return false;
	}
	
	m_countMutex.Lock();
	
	if (m_count > 0)
	{
		m_count--;
		m_countMutex.Unlock();
		return true;
	}
	m_waitMutex.Lock();
	m_countMutex.Unlock();
	m_isWaiting = true;
	m_waitMutex.Unlock();

#ifndef WIN32
	pthread_cond_wait(&m_cond, &m_mutex);
	pthread_mutex_unlock(&m_mutex);
#else
	WaitForSingleObject(m_eventObject,INFINITE);
	ResetEvent(m_eventObject);
#endif // WIN32
	return true;
}

bool MIPSignalWaiter::signal()
{
	if (!m_init)
	{
		// TODO error description
		return false;
	}
	m_countMutex.Lock();
	m_waitMutex.Lock();
	if (m_isWaiting)
	{
#ifndef WIN32
		pthread_cond_signal(&m_cond);
#else
		SetEvent(m_eventObject);
#endif // WIN32
		m_isWaiting = false;
	}
	else
		m_count++;
	
	m_waitMutex.Unlock();
	m_countMutex.Unlock();

	return true;
}

bool MIPSignalWaiter::clearSignalBuffers()
{
	if (!m_init)
	{
		// TODO error description
		return false;
	}
	
	m_countMutex.Lock();
	m_waitMutex.Lock();
	if (m_isWaiting)
	{
#ifndef WIN32
		pthread_cond_signal(&m_cond);
#else
		SetEvent(m_eventObject);
#endif // WIN32
	}
	m_count = 0;
	m_isWaiting = false;
	m_waitMutex.Unlock();
	m_countMutex.Unlock();
	return true;
}

