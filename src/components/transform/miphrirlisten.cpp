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

#include "mipconfig.h"
#include "miphrirlisten.h"
#include "miprawaudiomessage.h"
#include "mipdirectorybrowser.h"
#include "mipwavreader.h"
#include "mipcompat.h"
#include <iostream>

#include "mipdebug.h"

#define MIPHRIRLISTEN_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPHRIRLISTEN_ERRSTR_NOTINIT				"Not initialized"
#define MIPHRIRLISTEN_ERRSTR_NOHRIRFOUND			"No HRIR info was found in the specified directory or its subdirectories"
#define MIPHRIRLISTEN_ERRSTR_SUBJECTNUMBERNOTFOUND		"Specified subject number was not found"
#define MIPHRIRLISTEN_ERRSTR_UNSUPPORTEDHRIRFILE		"Detected an unsupported HRIR file: "
#define MIPHRIRLISTEN_ERRSTR_ERRORREADINGHRIRFILE		"Error reading HRIR file: "
#define MIPHRIRLISTEN_ERRSTR_ALREADYENTRYFORFILE		"Already an entry for data from HRIR file: "
#define MIPHRIRLISTEN_ERRSTR_HRIRFILEHASDIFFERENTSAMPLINGRATE	"HRIR file has different sampling rate than previous files: "
#define MIPHRIRLISTEN_ERRSTR_BADMESSAGE				"Bad message"
#define MIPHRIRLISTEN_ERRSTR_NOTMONO				"Only mono audio input is allowed"
#define MIPHRIRLISTEN_ERRSTR_INVALIDSAMPLINGRATE		"Incompatible sampling rate"
#define MIPHRIRLISTEN_ERRSTR_NOHRIRMATCHFOUND			"Couldn't find a suitable set of HRIR data"
#define MIPHRIRLISTEN_ERRSTR_CANTOPENINITIALDIRECTORY		"Can't open the initial directory"

MIPHRIRListen::MIPHRIRListen() : MIPHRIRBase("MIPHRIRListen")
{
	m_init = false;
}

MIPHRIRListen::~MIPHRIRListen()
{
	destroy();
}

bool MIPHRIRListen::init(const std::string &baseDirectory, int maxFilterLength, bool allowAmbientSound, bool useDistance)
{
	if (m_init)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_ALREADYINIT);
		return false;
	}

	MIPAudio3DBase::cleanUp();
	
	m_sampingRate = -1;
	if (!searchDirectory(baseDirectory, true))
	{
		clearHRIRSets();
		return false;
	}
	
	if (m_compensatedHRIRSets.empty())
	{
		if (m_rawHRIRSets.empty())
		{
			setErrorString(MIPHRIRLISTEN_ERRSTR_NOHRIRFOUND);
			return false;
		}
		else
			m_pCurHRIRSet = *(m_rawHRIRSets.begin());
	}
	else
		m_pCurHRIRSet = *(m_compensatedHRIRSets.begin());

	m_msgIt = m_messages.begin();
	m_prevIteration = -1;
	m_ambient = allowAmbientSound;
	m_useDistance = useDistance;
	m_maxFilterLength = maxFilterLength;
	m_init = true;
	
	return true;
}

bool MIPHRIRListen::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_NOTINIT);
		return false;
	}
	
	clearMessages();
	clearHRIRSets();
	MIPAudio3DBase::cleanUp();
	m_pCurHRIRSet = 0;
	m_init = false;

	return true;
}

bool MIPHRIRListen::selectHRIRSet(int subjectNumber, bool compensated)
{
	if (!m_init)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_NOTINIT);
		return false;
	}

	std::list<HRIRInfo *>::const_iterator it,endit;

	if (compensated)
	{
		it = m_compensatedHRIRSets.begin();
		endit = m_compensatedHRIRSets.end();
	}
	else
	{
		it = m_rawHRIRSets.begin();
		endit = m_rawHRIRSets.end();
	}
	
	bool found = false;
	
	while (it != endit && !found)
	{
		if ((*it)->getSubjectNumber() == subjectNumber)
		{
			found = true;
			m_pCurHRIRSet = (*it);
		}
		else
			it++;
	}

	if (!found)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_SUBJECTNUMBERNOTFOUND);
		return false;
	}

	return true;
}

bool MIPHRIRListen::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_NOTINIT);
		return false;
	}
	
	if (m_prevIteration != iteration)
	{
		clearMessages();
		m_prevIteration = iteration;
		expirePositionalInfo();
	}

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_BADMESSAGE);
		return false;
	}

	MIPRawFloatAudioMessage *pAudioMsg = (MIPRawFloatAudioMessage *)pMsg;

	if (pAudioMsg->getNumberOfChannels() != 1)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_NOTMONO);
		return false;
	}
	
	if (pAudioMsg->getSamplingRate() != m_sampingRate)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_INVALIDSAMPLINGRATE);
		return false;
	}

	// checks completed, look up position of source

	real_t distance, azimuth, elevation;

	if (!getPositionalInfo(pAudioMsg->getSourceID(),&azimuth,&elevation,&distance)) // no entry found
	{
		if (m_ambient) // allow the sound to be played; still has to be converted to stereo though
		{
			const float *pFrames = pAudioMsg->getFrames();
			int numFrames = pAudioMsg->getNumberOfFrames();
			float *pNewFrames = new float [numFrames*2];
			int i,j;

			for (i = 0, j = 0 ; i < numFrames ; i++, j += 2)
			{
				pNewFrames[j] = pFrames[i];
				pNewFrames[j+1] = pFrames[i];
			}
		
			// create the audio message

			MIPRawFloatAudioMessage *pNewMsg = new MIPRawFloatAudioMessage(m_sampingRate, 2, numFrames, pNewFrames, true);
			pNewMsg->setTime(pAudioMsg->getTime());
			pNewMsg->setSourceID(pAudioMsg->getSourceID());

			m_messages.push_back(pNewMsg);
			m_msgIt = m_messages.begin();
		}
	}
	else // entry found, add 3D effect
	{
		// look up the HRIR set to use

		HRIRData *pHRIRData = m_pCurHRIRSet->findBestMatch(azimuth,elevation);
		if (pHRIRData == 0)
		{
			setErrorString(MIPHRIRLISTEN_ERRSTR_NOHRIRMATCHFOUND);
			return false;
		}

		real_t factor = 1.0;

		if (m_useDistance)
		{
			if (distance < 0.50)
				distance = 0.50;

			factor = pHRIRData->getRadius()/distance;
		}

		float *pLeftChannel = pHRIRData->getLeftChannel();
		float *pRightChannel = pHRIRData->getRightChannel();
		const float *pFrames = pAudioMsg->getFrames();
		int numFrames = pAudioMsg->getNumberOfFrames();
		int numHRIRRightSamples = pHRIRData->getNumberOfRightSamples();
		int numHRIRLeftSamples = pHRIRData->getNumberOfLeftSamples();

		if (m_maxFilterLength > 0)
		{
			if (numHRIRLeftSamples > m_maxFilterLength) 
				numHRIRLeftSamples = m_maxFilterLength;
			if (numHRIRRightSamples > m_maxFilterLength) 
				numHRIRRightSamples = m_maxFilterLength;
		}
		
		int numRightSamples = numFrames + numHRIRRightSamples - 1;
		int numLeftSamples = numFrames + numHRIRLeftSamples - 1;

	
		int maxSamples = (numRightSamples > numLeftSamples) ? numRightSamples:numLeftSamples;
		float *pNewFrames = new float [maxSamples*2]; // stereo sound

		// Perform the convolution
		
		convolve(pNewFrames,maxSamples,(float)factor,pFrames,numFrames,pLeftChannel,numHRIRLeftSamples,pRightChannel,numHRIRRightSamples);
		
		// create the audio message

		MIPRawFloatAudioMessage *pNewMsg = new MIPRawFloatAudioMessage(m_sampingRate, 2, maxSamples, pNewFrames, true);
		pNewMsg->setTime(pAudioMsg->getTime());
		pNewMsg->setSourceID(pAudioMsg->getSourceID());

		m_messages.push_back(pNewMsg);
		m_msgIt = m_messages.begin();
	}
	
	return true;
}

bool MIPHRIRListen::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_NOTINIT);
		return false;
	}

	if (m_prevIteration != iteration)
	{
		clearMessages();
		m_prevIteration = iteration;
		expirePositionalInfo();
	}

	if (m_msgIt == m_messages.end())
	{
		*pMsg = 0;
		m_msgIt = m_messages.begin();
	}
	else
	{
		*pMsg = *m_msgIt;
		m_msgIt++;
	}

	return true;
}

void MIPHRIRListen::clearHRIRSets()
{
	std::list<HRIRInfo *>::const_iterator it;

	for (it = m_rawHRIRSets.begin() ; it != m_rawHRIRSets.end() ; it++)
		delete (*it);
	for (it = m_compensatedHRIRSets.begin() ; it != m_compensatedHRIRSets.end() ; it++)
		delete (*it);

	m_rawHRIRSets.clear();
	m_compensatedHRIRSets.clear();
}

bool MIPHRIRListen::searchDirectory(const std::string &path, bool reportOpenError)
{
	MIPDirectoryBrowser dirBrowser;	
	
	if (!dirBrowser.open(path))
	{
		if (!reportOpenError)
			return true;
		else
		{
			setErrorString(std::string(MIPHRIRLISTEN_ERRSTR_CANTOPENINITIALDIRECTORY) + dirBrowser.getErrorString());
			return false;
		}
	}

	std::list<std::string> directoryNames;
	std::string fileName;
	bool isDir;

	while (dirBrowser.getNextEntry(fileName,&isDir))
	{
		if (fileName != std::string("..") && fileName != std::string("."))
		{
			if (isDir)
				directoryNames.push_back(fileName);
			else
			{
				if (!processFile(path, fileName))
					return false;
			}
		}
	}

	dirBrowser.close();	
	
	std::list<std::string>::const_iterator it;

	for (it = directoryNames.begin() ; it != directoryNames.end() ; it++)
	{
#if (defined(WIN32) || defined(_WIN32_WCE))
		std::string fullPath = path + std::string("\\") + (*it);
#else
		std::string fullPath = path + std::string("/") + (*it);
#endif // WIN32 || _WIN32_WCE
		if (!searchDirectory(fullPath))
			return false;
	}

	return true;
}

bool MIPHRIRListen::processFile(const std::string &baseDir, const std::string &fileName)
{
	int subjectNumber, radius, azimuth, elevation;
	char typeChar;
	bool compensated;

#if defined(WIN32) && !defined(_WIN32_WCE) && (defined(_MSC_VER) && _MSC_VER >= 1400 )
	if (sscanf_s(fileName.c_str(),"IRC_%04d_%c_R%04d_T%03d_P%03d.wav",&subjectNumber,&typeChar,1,&radius,&azimuth,&elevation) != 5)
		return true; // ignore file
#else
	if (sscanf(fileName.c_str(),"IRC_%04d_%c_R%04d_T%03d_P%03d.wav",&subjectNumber,&typeChar,&radius,&azimuth,&elevation) != 5)
		return true; // ignore file
#endif 

	if (typeChar == 'C')
		compensated = true;
	else if (typeChar == 'R')
		compensated = false;
	else
	{
		setErrorString(std::string(MIPHRIRLISTEN_ERRSTR_UNSUPPORTEDHRIRFILE) + fileName);
		return false;
	}

	real_t radius2 = ((real_t)radius)/100.0; // convert to meters

	if (elevation > 180)
		elevation = elevation-360;

	if (elevation < -90 || elevation > 90)
	{
		setErrorString(std::string(MIPHRIRLISTEN_ERRSTR_UNSUPPORTEDHRIRFILE) + fileName);
		return false;
	}

#if (defined(WIN32) || defined(_WIN32_WCE))
	std::string fullPath = baseDir + std::string("\\") + fileName;
#else
	std::string fullPath = baseDir + std::string("/") + fileName;
#endif // WIN32 || _WIN32_WCE

	MIPWAVReader wavReader;
	
	if (!wavReader.open(fullPath)) // unable to open file, ignore it
		return true;

	if (wavReader.getNumberOfChannels() != 2)
	{
		setErrorString(std::string(MIPHRIRLISTEN_ERRSTR_UNSUPPORTEDHRIRFILE) + fileName);
		return false;
	}

	if (m_sampingRate == -1)
		m_sampingRate = wavReader.getSamplingRate();
	else
	{
		if (m_sampingRate != wavReader.getSamplingRate())
		{
			setErrorString(std::string(MIPHRIRLISTEN_ERRSTR_HRIRFILEHASDIFFERENTSAMPLINGRATE) + fileName);
			return false;
		}
	}
#define MIPHRIRLISTEN_BUFSIZE 1024
	
	float *pLeftChannel, *pRightChannel;
	float buf[MIPHRIRLISTEN_BUFSIZE];
	int pos = 0;
	
	pLeftChannel = new float [(size_t)wavReader.getNumberOfFrames()];
	pRightChannel = new float [(size_t)wavReader.getNumberOfFrames()];

	while (pos < wavReader.getNumberOfFrames())
	{
		int numleft = (int)wavReader.getNumberOfFrames()-pos;
		int num = (numleft > (MIPHRIRLISTEN_BUFSIZE/2))?(MIPHRIRLISTEN_BUFSIZE/2):numleft;
		int numRead;
		bool ret;
		
		ret = wavReader.readFrames(buf, num, &numRead);
		if ((!ret) || (numRead != num))
		{
			delete [] pLeftChannel;
			delete [] pRightChannel;
			setErrorString(std::string(MIPHRIRLISTEN_ERRSTR_ERRORREADINGHRIRFILE) + fileName);
			return false;
		}

		int i, j;
		
		for (i = 0, j = 0 ; i < num ; i++, j+= 2, pos++)
		{
			pLeftChannel[pos] = buf[j];
			pRightChannel[pos] = buf[j+1];
		}
	}
	
	// Frames read, add entry

	std::list<HRIRInfo *>::const_iterator it,endit;

	if (compensated)
	{
		it = m_compensatedHRIRSets.begin();
		endit = m_compensatedHRIRSets.end();
	}
	else
	{
		it = m_rawHRIRSets.begin();
		endit = m_rawHRIRSets.end();
	}
	
	bool found = false;
	HRIRInfo *pHRIRSet = 0;
	
	while (it != endit && !found)
	{
		if ((*it)->getSubjectNumber() == subjectNumber)
		{
			found = true;
			pHRIRSet = (*it);
		}
		else
			it++;
	}

	if (pHRIRSet == 0)
	{
		pHRIRSet = new HRIRInfo(subjectNumber);
		if (compensated)
			m_compensatedHRIRSets.push_front(pHRIRSet);
		else
			m_rawHRIRSets.push_front(pHRIRSet);
	}

	if (!pHRIRSet->addHRIRData(radius2, azimuth, elevation, pLeftChannel, (int)wavReader.getNumberOfFrames(), pRightChannel, (int)wavReader.getNumberOfFrames()))
	{
		delete [] pLeftChannel;
		delete [] pRightChannel;
		setErrorString(std::string(MIPHRIRLISTEN_ERRSTR_ALREADYENTRYFORFILE) + fileName);
		return false;
	}
	
//	std::cout << "Frames: " << wavReader.getNumberOfFrames() << " ";
//	std::cout << "Subject: " << subjectNumber << " ";
//	std::cout << "Radius: " << radius2 << " ";
//	std::cout << "Azimuth: " << azimuth << " ";
//	std::cout << "elevation: " << elevation << std::endl;
	
	return true;
}

void MIPHRIRListen::clearMessages()
{
	std::list<MIPRawFloatAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();
	m_msgIt = m_messages.begin();
}

bool MIPHRIRListen::getSubjectNumbers(std::list<int> &subjectNumbers, bool compensated)
{
	if (!m_init)
	{
		setErrorString(MIPHRIRLISTEN_ERRSTR_NOTINIT);
		return false;
	}

	subjectNumbers.clear();
	
	std::list<HRIRInfo *>::const_iterator it,endit;

	if (compensated)
	{
		it = m_compensatedHRIRSets.begin();
		endit = m_compensatedHRIRSets.end();
	}
	else
	{
		it = m_rawHRIRSets.begin();
		endit = m_rawHRIRSets.end();
	}
	
	for ( ; it != endit ; it++)
		subjectNumbers.push_back((*it)->getSubjectNumber());
	
	return true;
}

