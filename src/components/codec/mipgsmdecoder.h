/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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
 * \file mipgsmdecoder.h
 */

#ifndef MIPGSMDECODER_H

#define MIPGSMDECODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_GSM

#include "mipcomponent.h"
#include "miptime.h"
#if defined(WIN32) || defined(_WIN32_WCE)
	#include <hash_map>
#else
	#include <ext/hash_map>
#endif // Win32
#include <list>

class MIPAudioMessage;
struct gsm_state;

/** Decodes messages which contain GSM 06.10 encoded data.
 *  This component can be used to decompress data using the GSM codec. Input messages
 *  should be MIPEncodedAudioMessage instances with subtype MIPENCODEDAUDIOMESSAGE_TYPE_GSM.
 *  The component generates signed 16 bit native endian encoded raw audio messages.
 */
class MIPGSMDecoder : public MIPComponent
{
public:
	MIPGSMDecoder();
	~MIPGSMDecoder();

	/** Initialize the GSM decoder. */
	bool init();

	/** Clean up the GSM decoder. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();
	void expire();

	class GSMStateInfo
	{
	public:
		GSMStateInfo();
		~GSMStateInfo();

		gsm_state *getState()							{ return m_pState; }
		MIPTime getLastUpdateTime() const					{ return m_lastTime; }
		void setUpdateTime()							{ m_lastTime = MIPTime::getCurrentTime(); }
	private:
		MIPTime m_lastTime;
		gsm_state *m_pState;
	};

	bool m_init;

#if defined(WIN32) || defined(_WIN32_WCE)
	stdext::hash_map<uint64_t, GSMStateInfo *> m_gsmStates;
#else
	__gnu_cxx::hash_map<uint64_t, GSMStateInfo *, __gnu_cxx::hash<uint32_t> > m_gsmStates;
#endif // Win32
	
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_lastIteration;
	MIPTime m_lastExpireTime;
};	

#endif // MIPCONFIG_SUPPORT_GSM

#endif // MIPGSMDECODER_H

