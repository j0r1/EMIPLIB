/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2008  Hasselt University - Expertise Centre for
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
 * \file mipwavinput.h
 */

#ifndef MIPWAVINPUT_H

#define MIPWAVINPUT_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <string>

class MIPAudioMessage;
class MIPWAVReader;

/** A sound file input component.
 *  This component can be used to read audio from a WAV file.
 *  The component accepts MIPSYSTEMMESSAGE_ISTIME messages, so a timing component should 
 *  send its messages to the component. The output messages contain raw audio in floating 
 *  point or 16 bit signed native encoding.
 *  \note A frame consists of a number of samples (equal to the number of channels)
 *        sampled at the same instant. For example, in a stereo sound file, one
 *        frame would consist of two samples: one for the left channel and one for
 *        the right.
 */
class MIPWAVInput : public MIPComponent
{
public:
	MIPWAVInput();
	~MIPWAVInput();

	/** Opens a sound file.
	 *  With this function, a sound file can be opened for reading.
	 *  \param fname	The name of the sound file 
	 *  \param frames	The number of frames which should be read during each iteration.
	 *  \param loop		Flag indicating if the sound file should be played over and over again or just once.
	 *  \param intSamples	If \c true, 16 bit integer samples will be used. If \c false, floating point samples will be used.
	 */
	bool open(const std::string &fname, int frames, bool loop = true, bool intSamples = false);
	
	/** Opens a sound file.
	 *  With this function, a sound file can be opened for reading.
	 *  \param fname	The name of the sound file 
	 *  \param interval	During each iteration, a number of frames corresponding to the time interval described
	 *                      by this parameter are read.
	 *  \param loop		Flag indicating if the sound file should be played over and over again or just once.
	 *  \param intSamples	If \c true, 16 bit integer samples will be used. If \c false, floating point samples will be used.
	 */
	bool open(const std::string &fname, MIPTime interval, bool loop = true, bool intSamples = false);

	/** Closes the sound file.
	 *  Use this function to stop using a previously opened sound file.
	 */
	bool close();

	/** Returns the sampling rate of the sound file. */
	int getSamplingRate() const								{ if (m_pSndFile) return m_sampRate; return 0; }
	
	/** Returns the number of frames that will be read during each iteration. */
	int getNumberOfFrames() const								{ if (m_pSndFile) return m_numFrames; return 0; }

	/** Returns the number of channels in the sound file. */
	int getNumberOfChannels() const								{ if (m_pSndFile) return m_numChannels; return 0; }

	/** Sets the source ID for outgoing messages. */
	void setSourceID(uint64_t sourceID)							{ m_sourceID = sourceID; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	MIPWAVReader *m_pSndFile;
	int m_sampRate;
	int m_numFrames;
	int m_numChannels;
	bool m_intSamples;
	float *m_pFramesFloat;
	uint16_t *m_pFramesInt;
	MIPAudioMessage *m_pMsg;
	bool m_gotMessage;
	bool m_loop, m_eof;
	uint64_t m_sourceID;
};

#endif // MIPWAVINPUT_H

