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
 * \file mipavcodecdecoder.h
 */

#ifndef MIPAVCODECDECODER_H

#define MIPAVCODECDECODER_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_AVCODEC

#include "mipcomponent.h"
#include "miptime.h"

#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	#include <ffmpeg/avcodec.h>
#else
	extern "C" 
	{	
		#include <libavcodec/avcodec.h>
		#include <libswscale/swscale.h>
		#include <libavformat/avformat.h>
	}
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD

#if defined(WIN32) || defined(_WIN32_WCE)
	#include <hash_map>
#else
	#include <ext/hash_map>
#endif // Win32
#include <list>

class MIPRawYUV420PVideoMessage;

/** This component is a libavcodec based H.263+ decoder.
 *  This component is a libavcodec based H.263+ decoder. It accepts encoded video messages with
 *  subtype MIPENCODEDVIDEOMESSAGE_TYPE_H263P and creates raw video messages in YUV420P format.
 */
class MIPAVCodecDecoder : public MIPComponent
{
public:
	MIPAVCodecDecoder();
	~MIPAVCodecDecoder();

	/** Initialize the component.
	 *  Initialize the component.
	 *  \param waitForKeyframe If set to true, frames will only be output after a key frame
	 *                         has been received. Looks cleaner, but you may have to wait a
	 *                         bit longer to actually see something.
	 */
	bool init(bool waitForKeyframe);

	/** De-initialize the component. */
	bool destroy();
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);

	/** Initializes the libavcodec library.
	 *  This function initializes the libavcodec library. The library should only be initialized once
	 *  in an application.
	 */
	static void initAVCodec();
private:
	void clearMessages();
	void expire();

	class DecoderInfo
	{
	public:
#ifndef MIPCONFIG_SUPPORT_AVCODEC_OLD
		DecoderInfo(int w, int h, AVCodecContext *pContext, SwsContext *pSwsContext)
#else
		DecoderInfo(int w, int h, AVCodecContext *pContext)
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
		{
			m_width = w;
			m_height = h;
			m_pContext = pContext;
			m_lastTime = MIPTime::getCurrentTime();
#ifndef MIPCONFIG_SUPPORT_AVCODEC_OLD
			m_pSwsContext = pSwsContext;
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
			m_gotKeyframe = false;
		}

		~DecoderInfo()
		{
			avcodec_close(m_pContext);
			av_free(m_pContext);
#ifndef MIPCONFIG_SUPPORT_AVCODEC_OLD
			if (m_pSwsContext)
				sws_freeContext(m_pSwsContext);
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
		}

		int getWidth() const							{ return m_width; }
		int getHeight() const							{ return m_height; }
		AVCodecContext *getContext()						{ return m_pContext; }
#ifndef MIPCONFIG_SUPPORT_AVCODEC_OLD
		SwsContext *getSwsContext()						{ return m_pSwsContext; }
		void setSwsContext(SwsContext *pCtx)					{ m_pSwsContext = pCtx; }
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
		bool receivedKeyframe() const						{ return m_gotKeyframe; }
		void setReceivedKeyframe(bool f)					{ m_gotKeyframe = f; }

		MIPTime getLastUpdateTime() const					{ return m_lastTime; }
		void setLastUpdateTime(MIPTime t)					{ m_lastTime = t; }
	private:
		int m_width, m_height;
		AVCodecContext *m_pContext;
		MIPTime m_lastTime;
#ifndef MIPCONFIG_SUPPORT_AVCODEC_OLD
		SwsContext *m_pSwsContext;
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD
		bool m_gotKeyframe;
	};
	
	bool m_init;
	
#if defined(WIN32) || defined(_WIN32_WCE)
	stdext::hash_map<uint64_t, DecoderInfo *> m_decoderStates;
#else
	__gnu_cxx::hash_map<uint64_t, DecoderInfo *, __gnu_cxx::hash<uint32_t> > m_decoderStates;
#endif // Win32
	
	AVCodec *m_pCodec;
	AVFrame *m_pFrame;
#ifdef MIPCONFIG_SUPPORT_AVCODEC_OLD
	AVFrame *m_pFrameYUV420P;
#endif // MIPCONFIG_SUPPORT_AVCODEC_OLD

	std::list<MIPRawYUV420PVideoMessage *> m_messages;
	std::list<MIPRawYUV420PVideoMessage *>::const_iterator m_msgIt;
	int64_t m_lastIteration;
	MIPTime m_lastExpireTime;
	bool m_waitForKeyframe;
};

#endif // MIPCONFIG_SUPPORT_AVCODEC

#endif // MIPAVCODECDECODER_H

