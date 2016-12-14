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
 * \file mipfeedback.h
 */

#ifndef MIPFEEDBACK_H

#define MIPFEEDBACK_H

#include "mipconfig.h"
#include "miptime.h"

/** Message passed through a feedback chain.
 *  A MIPFeedback object is used in a feedback chain of a MIPComponentChain instance.
 *  Each object in the same chain can inspect and/or modify the information in the
 *  MIPFeedback instance.
 */
class MIPFeedback
{
public:
	MIPFeedback() : m_playbackStreamTime(0), m_playbackDelay(0)			{ m_playbackStreamTimeSet = false; }

	/** Sets current offset time in the playback stream.
	 *  Using this function, the current offset in the playback stream can be set. 
	 *  For example, this is set by the MIPAudioMixer and the data is inspected
	 *  by the MIPRTPDecoder. Only one object in a feedback chain can set this 
	 *  value.
	 */
	bool setPlaybackStreamTime(MIPTime t)						{ if (m_playbackStreamTimeSet) return false; m_playbackStreamTime = t; m_playbackStreamTimeSet = true; return true; }

	/** Returns true if the playback stream time has been set.
	 *  This function returns \c true if the playback stream time has been set and
	 *  \c false otherwise.
	 *  \sa MIPFeedback::setPlaybackStreamTime
	 */
	bool hasPlaybackStreamTime() const						{ return m_playbackStreamTimeSet; }

	/** Returns the playback stream offset.
	 *  Returns the playback stream offset, previously set by the 
	 *  MIPFeedback::setPlaybackStreamTime function.
	 */
	MIPTime getPlaybackStreamTime() const						{ return m_playbackStreamTime; }

	/** Adds \c t to the total amount of playback delay. */
	void addToPlaybackDelay(MIPTime t)						{ m_playbackDelay += t; }
	
	/** Returns the total playback delay. */
	MIPTime getPlaybackDelay() const						{ return m_playbackDelay; }
private:
	bool m_playbackStreamTimeSet;
	MIPTime m_playbackStreamTime;
	MIPTime m_playbackDelay;
};

#endif // MIPFEEDBACK_H

