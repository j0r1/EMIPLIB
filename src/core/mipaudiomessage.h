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
 * \file mipaudiomessage.h
 */

#ifndef MIPAUDIOMESSAGE_H

#define MIPAUDIOMESSAGE_H

#include "mipconfig.h"
#include "mipmediamessage.h"

/** Base class for encoded and raw audio messages.
 *  This class is the base class of both raw audio messages and encoded audio messages.
 */
class MIPAudioMessage : public MIPMediaMessage
{
protected:
	/** Constructor of the class.
	 *  Constructor of the class, meant to be used by subclasses.
	 *  \param isRaw Flag indicating is the message is intended for raw audio samples or
	 *               encoded audio.
	 *  \param msgSubtype Subtype of the message.
	 *  \param sampRate Sampling rate.
	 *  \param channels The number of channels.
	 *  \param frames The number of frames contained in the message.
	 */
	MIPAudioMessage(bool isRaw, uint32_t msgSubtype, int sampRate, int channels, int frames) : MIPMediaMessage(Audio, isRaw, msgSubtype)
													{ m_numChannels = channels; m_samplingRate = sampRate; m_numFrames = frames; }
public:
	~MIPAudioMessage()										{ }

	/** Returns the sampling rate. */
	int getSamplingRate() const									{ return m_samplingRate; }

	/** Returns the number of frames in the message. */
	int getNumberOfFrames() const									{ return m_numFrames; }

	/** Returns the number of channels. */
	int getNumberOfChannels() const									{ return m_numChannels; }

	/** Stores the sampling rate. */
	void setSamplingRate(int sampRate)								{ m_samplingRate = sampRate; }

	/** Stores the number of frames. */
	void setNumberOfFrames(int numFrames)								{ m_numFrames = numFrames; }

	/** Stores the number of channels. */
	void setNumberOfChannels(int numChannels)							{ m_numChannels = numChannels; }

	/** Copies the info stored in \c msg, including the MIPMediaMessage info. */
	void copyAudioInfoFrom(MIPAudioMessage &msg)							{ copyMediaInfoFrom(msg); m_samplingRate = msg.m_samplingRate; m_numChannels = msg.m_numChannels; m_numFrames = msg.m_numFrames; }
private:
	int m_numChannels;
	int m_numFrames;
	int m_samplingRate;
};

#endif // MIPAUDIOMESSAGE_H

