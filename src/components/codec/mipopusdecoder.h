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
 * \file mipopusdecoder.h
 */

#ifndef MIPOPUSDECODER_H

#define MIPOPUSDECODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_OPUS

#include "mipoutputmessagequeuewithstate.h"
#include "miptime.h"

class MIPAudioMessage;

/** Decodes messages which contain Opus encoded data.
 *  This component can be used to decompress data using the Opus codec. Input messages
 *  should be MIPEncodedAudioMessage instances with subtype MIPENCODEDAUDIOMESSAGE_TYPE_OPUS.
 *  The component generates floating point mono raw audio messages or signed 16 bit native endian
 *  encoded raw audio messages.
 */
class EMIPLIB_IMPORTEXPORT MIPOpusDecoder : public MIPOutputMessageQueueWithState
{
public:
	MIPOpusDecoder();
	~MIPOpusDecoder();

	/** Initialize the Opus decoder. 
	 *  Initialize the Opus decoder.
	 *  \param outputSamplingRate The audio messages produced by this component will use this sampling rate.
	 *  \param channels The audio messages produced by this component will use this number of channels.
	 *  \param useFloat If set to \c true, the raw audio data will be represented by floating point numbers,
	 *                  otherwise signed 16 bit values will be used.
	 */
	bool init(int outputSamplingRate, int channels, bool useFloat);

	/** Clean up the Opus decoder. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
private:
	class OpusStateInfo : public MIPStateInfo
	{
	public:
		OpusStateInfo(void *pState) 							{ m_pState = pState; }
		~OpusStateInfo();

		void *getState() const								{ return m_pState; }
	private:
		void *m_pState;
	};

	bool m_init;

	int m_outputSamplingRate;
	int m_outputChannels;
	bool m_useFloat;
};	

#endif // MIPCONFIG_SUPPORT_OPUS

#endif // MIPOPUSDECODER_H

