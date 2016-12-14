/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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
 * \file miprawaudiomessage.h
 */

#ifndef MIPRAWAUDIOMESSAGE_H

#define MIPRAWAUDIOMESSAGE_H

#include "mipconfig.h"
#include "mipaudiomessage.h"
#include "miptime.h"
#include <string.h>

/**
 * \def MIPRAWAUDIOMESSAGE_TYPE_FLOAT
 *      \brief A message with this subtype represents floating point encoded raw audio, stored in a
 *             MIPRawFloatAudioMessage instance.
 * \def MIPRAWAUDIOMESSAGE_TYPE_U8
 *      \brief A message with this subtype represents unsigned 8 bit encoded raw audio, stored in a
 *             MIPRawU8AudioMessage instance.
 * \def MIPRAWAUDIOMESSAGE_TYPE_U16LE
 *      \brief A message with this subtype represents unsigned 16 bit little endian encoded raw audio,
 *             stored in a MIPRaw16bitAudioMessage instance.
 * \def MIPRAWAUDIOMESSAGE_TYPE_U16BE
 *      \brief A message with this subtype represents unsigned 16 bit big endian encoded raw audio,
 *             stored in an MIPRaw16bitAudioMessage instance.
 * \def MIPRAWAUDIOMESSAGE_TYPE_S16LE
 *      \brief A message with this subtype represents signed 16 bit little endian encoded raw audio,
 *             stored in a MIPRaw16bitAudioMessage instance.
 * \def MIPRAWAUDIOMESSAGE_TYPE_S16BE
 *      \brief A message with this subtype represents signed 16 bit big endian encoded raw audio,
 *             stored in an MIPRaw16bitAudioMessage instance.
 * \def MIPRAWAUDIOMESSAGE_TYPE_S16
 *      \brief A message with this subtype represents signed 16 bit native endian encoded raw audio,
 *             stored in an MIPRaw16bitAudioMessage instance.
 * \def MIPRAWAUDIOMESSAGE_TYPE_U16
 *      \brief A message with this subtype represents unsigned 16 bit native endian encoded raw audio,
 *             stored in an MIPRaw16bitAudioMessage instance.
*/

#define MIPRAWAUDIOMESSAGE_TYPE_FLOAT								0x00000001
#define MIPRAWAUDIOMESSAGE_TYPE_U8								0x00000002
#define MIPRAWAUDIOMESSAGE_TYPE_U16LE								0x00000004
#define MIPRAWAUDIOMESSAGE_TYPE_U16BE								0x00000008
#define MIPRAWAUDIOMESSAGE_TYPE_S16LE								0x00000010
#define MIPRAWAUDIOMESSAGE_TYPE_S16BE								0x00000020
#define MIPRAWAUDIOMESSAGE_TYPE_S16								0x00000040
#define MIPRAWAUDIOMESSAGE_TYPE_U16								0x00000080

/** Container for floating point raw audio data. */
class MIPRawFloatAudioMessage : public MIPAudioMessage
{
public:
	/** Creates a MIPRawFloatAudioMessage instance.
	 *  Creates a MIPRawFloatAudioMessage instance.
	 *  \param sampRate Sampling rate.
	 *  \param numChannels Number of channels.
	 *  \param numFrames Number of frames.
	 *  \param pFrames The audio data.
	 *  \param deleteFrames Flag indicating if the data contained in \c pFrames should be
	 *                      deleted when this message is destroyed or when the data is replaced.
	 */
	MIPRawFloatAudioMessage(int sampRate, int numChannels, int numFrames, float *pFrames, bool deleteFrames) : MIPAudioMessage(true, MIPRAWAUDIOMESSAGE_TYPE_FLOAT, sampRate, numChannels, numFrames)
												{ m_pFrames = pFrames; m_deleteFrames = deleteFrames; }
	~MIPRawFloatAudioMessage()								{ if (m_deleteFrames) delete [] m_pFrames; }

	/** Returns the audio data. */
	const float *getFrames() const								{ return m_pFrames; }

	/** Stores audio data.
	 *  Stores audio data.
	 *  \param pFrames The audio data.
	 *  \param deleteFrames Flag indicating if the data contained in \c pFrames should be
	 *                      deleted when this message is destroyed or when the data is replaced.
	 */
	void setFrames(float *pFrames, bool deleteFrames)					{ if (m_deleteFrames) delete [] m_pFrames; m_pFrames = pFrames; m_deleteFrames = deleteFrames; }

	/** Create a copy of this message. */
	MIPMediaMessage *createCopy() const
	{
		size_t numSamples = getNumberOfFrames()*getNumberOfChannels();
		float *pFrames = new float [numSamples];

		memcpy(pFrames, m_pFrames, numSamples*sizeof(float));
		MIPMediaMessage *pMsg = new MIPRawFloatAudioMessage(getSamplingRate(), getNumberOfChannels(),
		                                                    getNumberOfFrames(), pFrames, true);
		pMsg->copyMediaInfoFrom(*this);
		return pMsg;
	}
private:
	float *m_pFrames;
	bool m_deleteFrames;
};

/** Container for unsigned eight-bit raw audio data. */
class MIPRawU8AudioMessage : public MIPAudioMessage
{
public:
	/** Creates a MIPRawU8AudioMessage instance.
	 *  Creates a MIPRawU8AudioMessage instance.
	 *  \param sampRate Sampling rate.
	 *  \param numChannels Number of channels.
	 *  \param numFrames Number of frames.
	 *  \param pFrames The audio data.
	 *  \param deleteFrames Flag indicating if the data contained in \c pFrames should be
	 *                      deleted when this message is destroyed or when the data is replaced.
	 */
	MIPRawU8AudioMessage(int sampRate, int numChannels, int numFrames, uint8_t *pFrames, bool deleteFrames) : MIPAudioMessage(true, MIPRAWAUDIOMESSAGE_TYPE_U8, sampRate, numChannels, numFrames)
												{ m_pFrames = pFrames; m_deleteFrames = deleteFrames; }
	~MIPRawU8AudioMessage()									{ if (m_deleteFrames) delete [] m_pFrames; }

	/** Returns the audio data. */
	uint8_t *getFrames() const								{ return m_pFrames; }

	/** Stores audio data.
	 *  Stores audio data.
	 *  \param pFrames The audio data.
	 *  \param deleteFrames Flag indicating if the data contained in \c pFrames should be
	 *                      deleted when this message is destroyed or when the data is replaced.
	 */
	void setFrames(uint8_t *pFrames, bool deleteFrames)					{ if (m_deleteFrames) delete [] m_pFrames; m_pFrames = pFrames; m_deleteFrames = deleteFrames; }
	
	/** Create a copy of this message. */
	MIPMediaMessage *createCopy() const
	{
		size_t numSamples = getNumberOfFrames()*getNumberOfChannels();
		uint8_t *pFrames = new uint8_t [numSamples];

		memcpy(pFrames, m_pFrames, numSamples*sizeof(uint8_t));
		MIPMediaMessage *pMsg = new MIPRawU8AudioMessage(getSamplingRate(), getNumberOfChannels(),
		                                                 getNumberOfFrames(), pFrames, true);
		pMsg->copyMediaInfoFrom(*this);
		return pMsg;
	}

private:
	uint8_t *m_pFrames;
	bool m_deleteFrames;
};

/** Container for sixteen-bit raw audio data. */
class MIPRaw16bitAudioMessage : public MIPAudioMessage
{
public:
	/** Used in constructor to specify sample encoding. */
	enum SampleEncoding { LittleEndian, BigEndian, Native };

	/** Creates a MIPRaw16bitAudioMessage instance.
	 *  Creates a MIPRaw16bitAudioMessage instance.
	 *  \param sampRate Sampling rate.
	 *  \param numChannels Number of channels.
	 *  \param numFrames Number of frames.
	 *  \param isSigned Flag indicating if the samples are stored as signed or unsigned data.
	 *  \param sampleEncoding Indicates if the samples are encoded in little endian, big endian or native format.
	 *  \param pFrames The audio data.
	 *  \param deleteFrames Flag indicating if the data contained in \c pFrames should be
	 *                      deleted when this message is destroyed or when the data is replaced.
	 */
	MIPRaw16bitAudioMessage(int sampRate, int numChannels, int numFrames, bool isSigned, SampleEncoding sampleEncoding, 
                                uint16_t *pFrames, bool deleteFrames) : MIPAudioMessage(true, calcSubtype(isSigned, sampleEncoding), sampRate, numChannels, numFrames)
												{ m_pFrames = pFrames; m_deleteFrames = deleteFrames; m_isSigned = isSigned; m_sampleEncoding = sampleEncoding; }
	~MIPRaw16bitAudioMessage()								{ if (m_deleteFrames) delete [] m_pFrames; }

	/** Returns the audio data. */
	uint16_t *getFrames() const								{ return m_pFrames; }

	/** Stores audio data.
	 *  Stores audio data.
	 *  \param isSigned Flag indicating if the samples are stored as signed or unsigned data.
	 *  \param sampleEncoding Indicates if the samples are encoded in little endian, big endian or native format.
	 *  \param pFrames The audio data.
	 *  \param deleteFrames Flag indicating if the data contained in \c pFrames should be
	 *                      deleted when this message is destroyed or when the data is replaced.
	 */
	void setFrames(bool isSigned, SampleEncoding sampleEncoding, uint16_t *pFrames, bool deleteFrames)
												{ if (m_deleteFrames) delete [] m_pFrames; m_pFrames = pFrames; m_deleteFrames = deleteFrames; setMessageSubtype(calcSubtype(isSigned, sampleEncoding)); m_sampleEncoding = sampleEncoding; m_isSigned = isSigned; }

	/** Returns \c true if the stored data uses a signed encoding, \c false otherwise. */
	bool isSigned() const									{ return m_isSigned; }

	/** Returns \c true if the stored data uses a big endian encoding, \c false otherwise. */
	bool isBigEndian() const								{ return (m_sampleEncoding == BigEndian)?true:false; }
	
	/** Returns \c true if the stored data uses a little endian encoding, \c false otherwise. */
	bool isLittleEndian() const								{ return (m_sampleEncoding == LittleEndian)?true:false; }

	/** Returns \c true if the stored data uses a native encoding, \c false otherwise. */
	bool isNative() const									{ return (m_sampleEncoding == Native)?true:false; }

	/** Returns sample encoding. */
	SampleEncoding getSampleEncoding() const						{ return m_sampleEncoding; } 

	/** Create a copy of this message. */
	MIPMediaMessage *createCopy() const
	{
		size_t numSamples = getNumberOfFrames()*getNumberOfChannels();
		uint16_t *pFrames = new uint16_t [numSamples];

		memcpy(pFrames, m_pFrames, numSamples*sizeof(uint16_t));
		MIPMediaMessage *pMsg = new MIPRaw16bitAudioMessage(getSamplingRate(), getNumberOfChannels(),
		                                                    getNumberOfFrames(), m_isSigned, 
								    m_sampleEncoding, pFrames, true);
		pMsg->copyMediaInfoFrom(*this);
		return pMsg;
	}
private:
	static inline uint32_t calcSubtype(bool isSigned, SampleEncoding sampleEncoding)
	{
		if (isSigned)
		{
			if (sampleEncoding == BigEndian)
				return MIPRAWAUDIOMESSAGE_TYPE_S16BE;
			if (sampleEncoding == LittleEndian)
				return MIPRAWAUDIOMESSAGE_TYPE_S16LE;
			return MIPRAWAUDIOMESSAGE_TYPE_S16;
		}
		if (sampleEncoding == BigEndian)
			return MIPRAWAUDIOMESSAGE_TYPE_U16BE;
		if (sampleEncoding == LittleEndian)
			return MIPRAWAUDIOMESSAGE_TYPE_U16LE;
		return MIPRAWAUDIOMESSAGE_TYPE_U16;
	}
	
	uint16_t *m_pFrames;
	bool m_deleteFrames;
	bool m_isSigned;
	SampleEncoding m_sampleEncoding;
};

#endif // MIPRAWAUDIOMESSAGE_H

