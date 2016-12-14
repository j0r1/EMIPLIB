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
 * \file mipspeexdecoder.h
 */

#ifndef MIPSPEEXDECODER_H

#define MIPSPEEXDECODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_SPEEX

#include "mipcomponent.h"
#include "miptime.h"
#if defined(WIN32) || defined(_WIN32_WCE)
	#include <hash_map>
#else
	#include <ext/hash_map>
#endif // Win32
#include <list>

class MIPAudioMessage;
struct SpeexBits;

/** Decodes messages which contain Speex encoded data.
 *  This component can be used to decompress data using the Speex codec. Input messages
 *  should be MIPEncodedAudioMessage instances with subtype MIPENCODEDAUDIOMESSAGE_TYPE_SPEEX.
 *  The component generates floating point mono raw audio messages or signed 16 bit native endian
 *  encoded raw audio messages.
 */
class MIPSpeexDecoder : public MIPComponent
{
public:
	MIPSpeexDecoder();
	~MIPSpeexDecoder();

	/** Initialize the Speex decoder. 
	 *  Initialize the Speex decoder.
	 *  \param floatSamples Flag indicating if floating point raw audio messages or signed 16 bit native
	 *                      endian raw audio messages should be generated.
	 */
	bool init(bool floatSamples = true);

	/** Clean up the Speex decoder. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();
	void expire();

	class SpeexStateInfo
	{
	public:
		enum SpeexBandWidth 
		{	 
			NarrowBand,
			WideBand,		
			UltraWideBand
		};

		SpeexStateInfo(SpeexBandWidth b);
		~SpeexStateInfo();

		SpeexBits *getBits()							{ return m_pBits; }
		void *getState()							{ return m_pState; }
		MIPTime getLastUpdateTime() const					{ return m_lastTime; }
		SpeexBandWidth getBandWidth() const					{ return m_bandWidth; }
		int getNumberOfFrames() const						{ return m_numFrames; }
		void setUpdateTime()							{ m_lastTime = MIPTime::getCurrentTime(); }
	private:
		MIPTime m_lastTime;
		void *m_pState;
		SpeexBits *m_pBits;
		SpeexBandWidth m_bandWidth;
		int m_numFrames;
	};

	bool m_init;

#if defined(WIN32) || defined(_WIN32_WCE)
	stdext::hash_map<uint64_t, SpeexStateInfo *> m_speexStates;
#else
	__gnu_cxx::hash_map<uint64_t, SpeexStateInfo *, __gnu_cxx::hash<uint32_t> > m_speexStates;
#endif // Win32
	
	std::list<MIPAudioMessage *> m_messages;
	std::list<MIPAudioMessage *>::const_iterator m_msgIt;
	int64_t m_lastIteration;
	MIPTime m_lastExpireTime;
	bool m_floatSamples;
};	

#endif // MIPCONFIG_SUPPORT_SPEEX

#endif // MIPSPEEXDECODER_H

