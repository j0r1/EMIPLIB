/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2017  Hasselt University - Expertise Centre for
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
 * \file mipalsaoutput.h
 */

#ifndef MIPALSAOUTPUT_H

#define MIPALSAOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_ALSA

#include "mipcomponent.h"
#include "miptime.h"
#include "mipsignalwaiter.h"
#include <alsa/asoundlib.h>
#include <jthread/jthread.h>
#include <string>

class MIPStreamBuffer;

/** An Advanced Linux Sound Architecture (ALSA) soundcard output component.
 *  This component uses the Advanced Linux Sound Architecture (ALSA) system to provide
 *  soundcard output functions. The component accepts floating point raw audio messages
 *  or signed 16 bit integer encoded raw audio messages and does not generate any messages itself.
 */
class MIPAlsaOutput : public MIPComponent, private jthread::JThread
{
public:
	MIPAlsaOutput();
	~MIPAlsaOutput();

	/** Opens the soundcard device.
	 *  With this function, the soundcard output component opens and initializes the 
	 *  soundcard device.
	 *  \param sampRate The sampling rate.
	 *  \param channels The number of channels.
	 *  \param device The name of the device to open.
	 *  \param blockTime Audio data with a length corresponding to this parameter is
	 *                   sent to the soundcard device during each iteration.
	 *  \param floatSamples Flag indicating if floating point samples or integer samples
	 *                      should be used.
	 */
	bool open(int sampRate, int channels, MIPTime blockTime = MIPTime(0.020), bool floatSamples = true,
			  const std::string device = std::string("plughw:0,0"));

	/** Closes the soundcard device.
	 *  This function closes the previously opened soundcard device.
	 */
	bool close();
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void *Thread();
	
	snd_pcm_t *m_pDevice;
	snd_pcm_hw_params_t *m_pHwParameters;
	bool m_floatSamples;
	int m_sampRate;
	int m_channels;
	int m_blockFrames;
	int m_blockSize;

	MIPStreamBuffer *m_pBuffer;
	MIPSignalWaiter m_sigWait;
	jthread::JMutex m_stopMutex;
	bool m_stopLoop;
};

#endif // MIPCONFIG_SUPPORT_ALSA

#endif // MIPALSAOUTPUT_H

