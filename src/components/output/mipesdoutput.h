/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2011  Hasselt University - Expertise Centre for
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
 * \file mipesdoutput.h
 */

#ifndef MIPESDOUTPUT_H

#define MIPESDOUTPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_ESD

#include "mipcomponent.h"
#include "miptime.h"
#include <string>

/** An Enlightened Sound Daemon (ESD) soundcard output component.
 *  This component uses the Enlightened Sound Daemon (ESD) system to provide
 *  soundcard output functions. The component accepts integer raw audio messages
 *  (16 bit native endian) and does not generate any messages itself.
 */
class MIPEsdOutput : public MIPComponent
{
public:
	MIPEsdOutput();
	~MIPEsdOutput();

	/** Opens the soundcard device.
	 *  With this function, the soundcard output component opens and initializes the 
	 *  soundcard device.
	 *  \param sampRate The sampling rate.
	 *	\param channels The number of channels.
	 *  \param sampWidth The width of a sample in bits.
	 *  \param blockTime Audio data with a length corresponding to this parameter is
	 *                   sent to the soundcard device during each iteration.
	 *  \param arrayTime The amount of memory allocated to internal buffers,
	 *                   specified as a time interval. Note that this does not correspond
	 *                   to the amount of buffering introduced by this component.
	 */
	bool open(int sampRate, int channels, int sampWidth = 16, MIPTime blockTime = MIPTime(0.020), MIPTime arrayTime = MIPTime(10.0));

	/** Returns the sampling rate used by ESD */
	int getSamplingRate() const									{ return m_sampRate; }

	/** Returns the number of channels used by ESD */
	int getNumberOfChannels() const									{ return m_channels; }

	/** Returns the sample width used by ESD */
	int getSampleWidth() const									{ return m_sampWidth; }

	/** Closes the soundcard device.
	 *  This function closes the previously opened soundcard device.
	 */
	bool close();
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	
	bool m_opened;
	int m_sampRate;
	int m_channels;
	int m_esd_handle;
	int m_sampWidth;
};

#endif // MIPCONFIG_SUPPORT_ESD

#endif // MIPESDOUTPUT_H

