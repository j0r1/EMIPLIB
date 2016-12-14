/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Hasselt University - Expertise Centre for
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
#include <speex/speex.h>
#if defined(WIN32) || defined(_WIN32_WCE)
	#include <hash_map>
#else
	#include <ext/hash_map>
#endif // Win32
#include <list>

class MIPAudioMessage;

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
		/** Used to select speex encoding type. */
		enum SpeexBandWidth 
		{	 
 			/** Narrow band mode (8000 Hz) */
			NarrowBand,
		 	/** Wide band mode (16000 Hz) */
			WideBand,		
	 		/** Ultra wide band mode (32000 Hz) */
			UltraWideBand
		};

		SpeexStateInfo(SpeexBandWidth b) 
		{ 
			m_lastTime = MIPTime::getCurrentTime(); 
			speex_bits_init(&m_bits); 
			if (b == NarrowBand)
				m_pState = speex_decoder_init(&speex_nb_mode);
			else if (b == WideBand)
				m_pState = speex_decoder_init(&speex_wb_mode);
			else
				m_pState = speex_decoder_init(&speex_uwb_mode);
			m_bandWidth = b;
			speex_decoder_ctl(m_pState, SPEEX_GET_FRAME_SIZE, &m_numFrames); 
		}
		
		~SpeexStateInfo() 
		{ 
			speex_bits_destroy(&m_bits);
			speex_decoder_destroy(m_pState);
		}

		SpeexBits *getBits()							{ return &m_bits; }
		void *getState()							{ return m_pState; }
		MIPTime getLastUpdateTime() const					{ return m_lastTime; }
		SpeexBandWidth getBandWidth() const					{ return m_bandWidth; }
		int getNumberOfFrames() const						{ return m_numFrames; }
		void setUpdateTime()							{ m_lastTime = MIPTime::getCurrentTime(); }
	private:
		MIPTime m_lastTime;
		void *m_pState;
		SpeexBits m_bits;
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

