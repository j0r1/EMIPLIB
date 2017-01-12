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
 * \file mipqt5audiooutput.h
 */

#ifndef MIPQT5AUDIOOUTPUT_H

#define MIPQT5AUDIOOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_QT5

#include "mipcomponent.h"
#include "miptime.h"
#include <jthread/jmutex.h>
#include <string>

class QAudioDeviceInfo;
class QAudioOutput;
class QIODevice;
class MIPStreamBuffer;

/** TODO
 *  note: qt event loop must be running!
 */
class EMIPLIB_IMPORTEXPORT MIPQt5AudioOutput : public MIPComponent
{
public:
	MIPQt5AudioOutput();
	~MIPQt5AudioOutput();

	/** TODO
	 */
	bool open(int sampleRate = 48000, int channels = 2, MIPTime interval = MIPTime(0.020),
			  const QAudioDeviceInfo *pAudioDevice = nullptr, int maxQueuedBuffers = 3);

	/** Cleans up the component. */
	bool close();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:	
	bool m_init;
	int m_sampRate, m_numChannels;
	int m_bytesPerFrame;
	int m_maxQueuedBuffers;
	MIPStreamBuffer *m_pBuffer;
	QIODevice *m_pIODev;
	QAudioOutput *m_pAudioOutput;
};

#endif // MIPCONFIG_SUPPORT_QT5

#endif // MIPQT5AUDIOOUTPUT_H

