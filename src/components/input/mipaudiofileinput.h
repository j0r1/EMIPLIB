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
 * \file mipaudiofileinput.h
 */
#ifndef MIPAUDIOFILEINPUT_H

#define MIPAUDIOFILEINPUT_H

#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_AUDIOFILE

#include "mipcomponent.h"
#include "miprawaudiomessage.h"
#include "miptime.h"
#include <audiofile.h>
#include <string>

/** A sound file input component.
 *  This component can be used to read audio from a file. The component uses the
 *  \c libaudiofile library to accomplish this. This component accepts MIPSYSTEMMESSAGE_TYPE_ISTIME
 *  messages, so a timing component should sent its messages to the component. The
 *  output messages contain raw audio in 16 bit integer encoding.
 *  \note A frame consists of a number of samples (equal to the number of channels)
 *        sampled at the same instant. For example, in a stereo sound file, one
 *        frame would consist of two samples: one for the left channel and one for
 *        the right.
 */
class MIPAudioFileInput : public MIPComponent
{
public:
	MIPAudioFileInput();
	~MIPAudioFileInput();

	/** Opens a sound file.
	 *  With this function, a sound file can be opened for reading.
	 *  \param fname	The name of the sound file 
	 *  \param frames	The number of frames which should be read during each iteration.
	 *  \param loop		Flag indicating if the sound file should be played over and over again or just once.
	 */
	bool open(const std::string &fname, int frames, bool loop = true);
	
	/** Opens a sound file.
	 *  With this function, a sound file can be opened for reading.
	 *  \param fname	The name of the sound file 
	 *  \param interval	During each iteration, a number of frames corresponding to the time interval described
	 *                      by this parameter are read.
	 *  \param loop		Flag indicating if the sound file should be played over and over again or just once.
	 */
	bool open(const std::string &fname, MIPTime interval, bool loop = true);

	/** Closes the sound file.
	 *  Use this function to stop using a previously opened sound file.
	 */
	bool close();

	/** Returns the sampling rate of the sound file. */
	int getSamplingRate() const								{ if (m_audiofileHandle) return m_sampRate; return 0; }
	
	/** Returns the number of frames that will be read during each iteration. */
	int getNumberOfFrames() const								{ if (m_audiofileHandle) return m_numFrames; return 0; }

	/** Returns the number of channels in the sound file. */
	int getNumberOfChannels() const								{ if (m_audiofileHandle) return m_numChannels; return 0; }

	/** Returns the sample encoding used in generated raw audio messages. */
	MIPRaw16bitAudioMessage::SampleEncoding getSampleEncoding() const			{ if (m_audiofileHandle) return m_sampleEncoding; return MIPRaw16bitAudioMessage::Native; }

	/** Selects which source ID will be set in outgoing audio messages. */
	void setSourceID(uint64_t sourceID)							{ m_sourceID = sourceID; }

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	AFfilehandle m_audiofileHandle;
	int m_sampRate;
	int m_numFrames;
	int m_numChannels;
	uint16_t *m_pFrames;
	MIPRaw16bitAudioMessage *m_pMsg;
	MIPRaw16bitAudioMessage::SampleEncoding m_sampleEncoding;
	bool m_gotMessage;
	bool m_loop, m_eof;
	uint64_t m_sourceID;
};

#endif // MIPCONFIG_SUPPORT_AUDIOFILE

#endif // MIPAUDIOFILEINPUT_H

