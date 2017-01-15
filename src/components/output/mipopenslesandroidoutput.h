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
 * \file mipopenslesandroidoutput.h
 */

#ifndef MIPOPENSLESANDROIDOUTPUT_H

#define MIPOPENSLESANDROIDOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_OPENSLESANDROID

#include "mipcomponent.h"
#include "miptime.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <jthread/jthread.h>
#include <vector>
#include <atomic>

class MIPStreamBuffer;

/** TODO
 */
class EMIPLIB_IMPORTEXPORT MIPOpenSLESAndroidOutput : public MIPComponent
{
public:
	MIPOpenSLESAndroidOutput();
	~MIPOpenSLESAndroidOutput();

	/** TODO
	*/
	bool open(int sampRate, int channels, MIPTime interval);

	/** Closes a previously opened component. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);

private:
	bool m_init;

	int m_sampRate;
	int m_channels;
	int m_blockSize;
	MIPStreamBuffer *m_pBuffer;

	class AudioThread : public jthread::JThread
	{
	public:
		AudioThread(MIPStreamBuffer *pBuffer, int blockSize, int sampRate, int channels, MIPTime interval);
		~AudioThread();

		void *Thread();
		std::string getErrorString() const { return m_errorString; }
		bool successFullyStarted() const { return m_successFullyStarted; }
	private:
		void bufferQueueCallback(SLAndroidSimpleBufferQueueItf caller);
		static void staticBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext);

		std::string m_errorString;
		std::atomic_bool m_stopFlag, m_successFullyStarted;
		MIPStreamBuffer *m_pBuffer;
		int m_blockSize;
		int m_sampRate, m_channels;
		MIPTime m_interval;

		SLAndroidSimpleBufferQueueItf m_playerBuffer;
		std::vector<uint8_t> m_empty, m_buf;
	};

	AudioThread *m_pThread;
};

#endif // MIPCONFIG_SUPPORT_OPENSLESANDROID

#endif // MIPOPENSLESANDROIDOUTPUT_H
