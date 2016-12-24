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
 * \file mipsilkdecoder.h
 */

#ifndef MIPSILKDECODER_H

#define MIPSILKDECODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SILK

#include "mipcomponent.h"
#include "miptime.h"
#include <unordered_map>
#include <list>

class MIPAudioMessage;

/** Decodes messages which contain SILK encoded data.
 *  This component can be used to decompress data using the SILK codec. Input messages
 *  should be MIPEncodedAudioMessage instances with subtype MIPENCODEDAUDIOMESSAGE_TYPE_SILK.
 *  The component generates signed 16 bit native endian encoded raw audio messages.
 */
class EMIPLIB_IMPORTEXPORT MIPSILKDecoder : public MIPComponent
{
public:
	MIPSILKDecoder();
	~MIPSILKDecoder();

	/** Initialize the SILK decoder. */
	bool init(int outputSamplingRate);

	/** Clean up the SILK decoder. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();
	void expire();

	class SILKStateInfo
	{
	public:
		SILKStateInfo(uint8_t *pState)						{ m_pDecoderState = pState; m_lastTime = MIPTime::getCurrentTime(); }
		~SILKStateInfo()							{ delete [] m_pDecoderState; }

		uint8_t *getDecoderState()						{ return m_pDecoderState; }
		MIPTime getLastUpdateTime() const					{ return m_lastTime; }
		void setUpdateTime()							{ m_lastTime = MIPTime::getCurrentTime(); }
	private:
		MIPTime m_lastTime;
		uint8_t *m_pDecoderState;
	};

	bool m_init;

	std::unordered_map<uint64_t, SILKStateInfo *> m_silkStates;
	
	int m_outputSamplingRate;
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_lastIteration;
	MIPTime m_lastExpireTime;
};	

#endif // MIPCONFIG_SUPPORT_SILK

#endif // MIPSILKDECODER_H

