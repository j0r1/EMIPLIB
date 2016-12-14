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
 * \file miphrirlisten.h
 */

#ifndef MIPHRIRLISTEN_H

#define MIPHRIRLISTEN_H

#include "mipconfig.h"
#include "miphrirbase.h"
#include <cmath>
#include <list>

class MIPRawFloatAudioMessage;

/** Creates 3D sound based on HRIR info from the LISTEN project.
 *  Using this component, raw floating point mono audio messages can be converted into
 *  stereo raw floating point audio messages. The sound in the output messages will
 *  have a 3D effect, based upon your own location and the location of the sound source.
 */
class MIPHRIRListen : public MIPHRIRBase
{
public:
	MIPHRIRListen();
	~MIPHRIRListen();

	/** Initializes the component.
	 *  This function initializes the 3D audio component.
	 *  \param baseDirectory In this directory and its subdirectories, the component
	 *                       will look for WAV files containing data from the LISTEN
	 *                       project.
	 *  \param maxFilterLength If larger than zero, only the first \c maxFilterLength
	 *                         bytes of left and right filters are used in the convolution
	 *                         product. Can be used to make a tradeoff between 3D sound
	 *                         quality and CPU load.
	 *  \param allowAmbientSound If set to \c false, messages corresponding to sources for
	 *                           which no positional information can be found are ignored.
	 *                           If set to \c true, they're simply converted to stereo sound.
	 *  \param useDistance If set to \c true, the distance of a source is used to adjust the
	 *                     sound amplitude.
	 */
	bool init(const std::string &baseDirectory, int maxFilterLength = 48, 
	          bool allowAmbientSound = true, bool useDistance = true);

	/** De-initializes the component. */
	bool destroy();

	/** Select a specific HRIR data set.
	 *  When multiple HRIR data sets can be found in the base directory specified in the
	 *  MIPHRIRListen::init function, this function can be used to select a specific set.
	 *  \param subjectNumber The subject number belonging to a specific set.
	 *  \param compensated Flag indicated if compensated data or raw data should be used.
	 */
	bool selectHRIRSet(int subjectNumber, bool compensated = true);

	/** Returns the sampling rate which is used by this component. */
	int getSamplingRate() const								{ return m_sampingRate; }

	/** Returns a list of the available subject numbers. */
	bool getSubjectNumbers(std::list<int> &subjectNumbers, bool compensated = true);
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();
	void clearHRIRSets();
	bool searchDirectory(const std::string &path, bool reportOpenError = false);
	bool processFile(const std::string &baseDir, const std::string &fileName);
	
	bool m_init;
	bool m_ambient;
	bool m_useDistance;
	int m_sampingRate;
	int m_maxFilterLength;
	
	std::list<HRIRInfo *> m_rawHRIRSets;
	std::list<HRIRInfo *> m_compensatedHRIRSets;
	
	HRIRInfo *m_pCurHRIRSet;

	int64_t m_prevIteration;
	std::list<MIPRawFloatAudioMessage *> m_messages;
	std::list<MIPRawFloatAudioMessage *>::const_iterator m_msgIt;
};

#endif // MIPHRIRLISTEN_H

