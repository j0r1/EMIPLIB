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
 * \file miplpcdecoder.h
 */

#ifndef MIPLPCDECODER_H

#define MIPLPCDECODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_LPC

#include "mipcomponent.h"
#include "miptime.h"
#include <unordered_map>
#include <list>

class MIPAudioMessage;
class LPCDecoder;

/** Decodes messages which contain LPC encoded data.
 *  This component can be used to decompress data using the LPC codec. Input messages
 *  should be MIPEncodedAudioMessage instances with subtype MIPENCODEDAUDIOMESSAGE_TYPE_LPC.
 *  The component generates signed 16 bit native endian encoded raw audio messages.
 */
class EMIPLIB_IMPORTEXPORT MIPLPCDecoder : public MIPComponent
{
public:
	MIPLPCDecoder();
	~MIPLPCDecoder();

	/** Initialize the LPC decoder. */
	bool init();

	/** Clean up the LPC decoder. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();
	void expire();

	class LPCStateInfo
	{
	public:
		LPCStateInfo();
		~LPCStateInfo();

		LPCDecoder *getDecoder()						{ return m_pDecoder; }
		MIPTime getLastUpdateTime() const					{ return m_lastTime; }
		void setUpdateTime()							{ m_lastTime = MIPTime::getCurrentTime(); }
	private:
		MIPTime m_lastTime;
		LPCDecoder *m_pDecoder;
	};

	bool m_init;

	std::unordered_map<uint64_t, LPCStateInfo *> m_lpcStates;
	
	int *m_pFrameBuffer;
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_lastIteration;
	MIPTime m_lastExpireTime;
};	

#endif // MIPCONFIG_SUPPORT_LPC

#endif // MIPLPCDECODER_H

