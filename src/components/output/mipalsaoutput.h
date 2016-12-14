/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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
#include <alsa/asoundlib.h>
#include <jthread.h>
#include <string>

/** An Advanced Linux Sound Architecture (ALSA) soundcard output component.
 *  This component uses the Advanced Linux Sound Architecture (ALSA) system to provide
 *  soundcard output functions. The component accepts floating point raw audio messages
 *  or signed 16 bit integer encoded raw audio messages and does not generate any messages itself.
 */
class MIPAlsaOutput : public MIPComponent, private JThread
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
	 *  \param arrayTime The amount of memory allocated to internal buffers,
	 *                   specified as a time interval. Note that this does not correspond
	 *                   to the amount of buffering introduced by this component.
	 *  \param floatSamples Flag indicating if floating point samples or integer samples
	 *                      should be used.
	 */
	bool open(int sampRate, int channels, const std::string device = std::string("plughw:0,0"), 
                  MIPTime blockTime = MIPTime(0.020), 
		  MIPTime arrayTime = MIPTime(10.0),
		  bool floatSamples = true);

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
	int m_sampRate;
	int m_channels;
	float *m_pFrameArrayFloat;
	uint16_t *m_pFrameArrayInt;
	bool m_floatSamples;
	size_t m_frameArrayLength;
	size_t m_currentPos, m_nextPos;
	size_t m_blockLength, m_blockFrames;
	MIPTime m_delay;
	MIPTime m_distTime, m_blockTime;
	JMutex m_frameMutex, m_stopMutex;
	bool m_stopLoop;
};

#endif // MIPCONFIG_SUPPORT_ALSA

#endif // MIPALSAOUTPUT_H

