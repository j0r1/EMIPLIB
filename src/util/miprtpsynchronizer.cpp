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

#include "mipconfig.h"
#include "miprtpsynchronizer.h"
#include <iostream>
#include <cstdlib>

#include "mipdebug.h"

#define MIPRTPSYNCHRONIZER_ERRSTR_IDNOTFOUND		"Specified ID was not found in the table"
#define MIPRTPSYNCHRONIZER_ERRSTR_INVALIDCNAMELENGTH	"An invalid CNAME length was specified"

MIPRTPSynchronizer::MIPRTPSynchronizer()
{
	int status;
	
	if ((status = m_mutex.Init()) < 0)
	{
		std::cerr << "Error: can't initialize component mutex (JMutex error code " << status << ")" << std::endl;
		exit(-1);
	}

	clear();
}

MIPRTPSynchronizer::~MIPRTPSynchronizer()
{
	clear();
}

void MIPRTPSynchronizer::clear()
{
	std::map<CNameInfo, StreamGroup *, CNameInfo>::iterator it1;
	std::list<uint8_t *> cnames;
	
	for (it1 = m_cnameTable.begin() ; it1 != m_cnameTable.end() ; it1++)
	{
		cnames.push_back((*it1).first.getCName());
		delete (*it1).second;
	}
	m_cnameTable.clear();

	std::list<uint8_t *>::iterator it2;
	for (it2 = cnames.begin() ; it2 != cnames.end() ; it2++)
	{
		uint8_t *pCName = (*it2);
		delete [] pCName;
	}
	
	std::map<int64_t, StreamInfo *>::iterator it3;

	for (it3 = m_streamIDTable.begin() ; it3 != m_streamIDTable.end() ; it3++)
		delete (*it3).second;
	m_streamIDTable.clear();

	m_tolerance = MIPTime(0.100);
}

bool MIPRTPSynchronizer::registerStream(const uint8_t *pCName, size_t cnameLength, real_t timestampUnit, int64_t *streamID)
{
	if (cnameLength <= 0)
	{
		setErrorString(MIPRTPSYNCHRONIZER_ERRSTR_INVALIDCNAMELENGTH);
		return false;
	}

//	char tmp[1024];
//	memcpy(tmp,pCName,cnameLength);
//	tmp[cnameLength] = 0;
//	std::cout << std::string(tmp) << std::endl;
	
	int64_t id = m_nextStreamID++;
	CNameInfo searchCNameInf(const_cast<uint8_t *>(pCName),cnameLength);
	
	std::map<CNameInfo, StreamGroup *, CNameInfo>::iterator it1 = m_cnameTable.find(searchCNameInf);
	StreamGroup *pGroup;

	if (it1 == m_cnameTable.end()) // no entry exists
	{
		uint8_t *pNewCName = new uint8_t [cnameLength];

		memcpy(pNewCName,pCName,cnameLength);
		pGroup = new StreamGroup();
		
		CNameInfo cnameInf(pNewCName, cnameLength);
		
		m_cnameTable[cnameInf] = pGroup;
	}
	else // entry already exists
	{
		pGroup = (*it1).second;
	}

	StreamInfo *pStreamInf = new StreamInfo(pGroup, timestampUnit);
	pGroup->streams().push_back(pStreamInf);
	pGroup->setStreamsChanged(true);

	m_streamIDTable[id] = pStreamInf;
	*streamID = id;

//	std::cout << "Registered stream ID: " << id << std::endl;

	return true;
}

bool MIPRTPSynchronizer::unregisterStream(int64_t streamID)
{
	//std::cout << "unregisterStream: " << streamID << std::endl;
	
	std::map<int64_t, StreamInfo *>::iterator it1 = m_streamIDTable.find(streamID);

	if (it1 == m_streamIDTable.end())
	{
		setErrorString(MIPRTPSYNCHRONIZER_ERRSTR_IDNOTFOUND);
		return false;
	}
	
	StreamInfo *pStreamInf = (*it1).second;
	m_streamIDTable.erase(it1);

	StreamGroup *pGroup = pStreamInf->getGroup();
	std::list<StreamInfo *>::iterator it2 = pGroup->streams().begin();
	bool done = false;
	
	while (!done && it2 != pGroup->streams().end())
	{
		if ((*it2) == pStreamInf)
		{
			done = true;
			pGroup->streams().erase(it2);
			pGroup->setStreamsChanged(true);
		}
		else
			it2++;
	}

	delete pStreamInf;

	if (pGroup->streams().empty())
	{
		std::map<CNameInfo, StreamGroup *, CNameInfo>::iterator it = m_cnameTable.begin();
		bool done = false;
		
		while (!done && it != m_cnameTable.end())
		{
			if ((*it).second == pGroup)
			{
				done = true;
				
				std::map<CNameInfo, StreamGroup *, CNameInfo>::iterator it2 = it;
				it++;

				uint8_t *pCName = (*it2).first.getCName();
				
				m_cnameTable.erase(it2);
				delete pGroup;
				delete [] pCName;
			}
			else
				it++;
		}
	}
	
	return true;
}

bool MIPRTPSynchronizer::setStreamInfo(int64_t streamID, MIPTime SRwallclock, uint32_t SRtimestamp, 
                                       uint32_t curTimestamp, MIPTime outputStreamOffset, MIPTime totalComponentDelay)
{
	std::map<int64_t, StreamInfo *>::iterator it1 = m_streamIDTable.find(streamID);

//	std::cout << "Setting stream info for ID: " << streamID << std::endl;
//	std::cout << "  SRwallclock:              " << SRwallclock.getString() << std::endl;
//	std::cout << "  SRtimestamp:              " << SRtimestamp << std::endl;
//	std::cout << "  curTimestamp:             " << curTimestamp << std::endl;
//	std::cout << "  outputStreamOffset:       " << outputStreamOffset.getString() << std::endl;
//	std::cout << "  totalComponentDelay:      " << totalComponentDelay.getString() << std::endl << std::endl;

	if (it1 == m_streamIDTable.end())
	{
		setErrorString(MIPRTPSYNCHRONIZER_ERRSTR_IDNOTFOUND);
		return false;
	}
	
	StreamInfo *pStreamInf = (*it1).second;

	pStreamInf->setInfo(SRwallclock, SRtimestamp, curTimestamp, outputStreamOffset, totalComponentDelay);
	return true;
}

MIPTime MIPRTPSynchronizer::calculateSynchronizationOffset(int64_t streamID)
{
	std::map<int64_t, StreamInfo *>::iterator it1 = m_streamIDTable.find(streamID);

	if (it1 == m_streamIDTable.end())
		return MIPTime(0);

	StreamInfo *pStreamInf = (*it1).second;
	StreamGroup *pGroup = pStreamInf->getGroup();
	bool recalc = false;
	
	if (pGroup->didStreamsChange())		
		recalc = true;
	else
	{
		if ((MIPTime::getCurrentTime().getValue() - pGroup->getLastCalculationTime().getValue()) > 5.0) // only perform calculation each five seconds
			recalc = true;
	}

	if (!recalc)
	{
//		std::cout << "Stream " << streamID << ": sync offset " << pStreamInf->getSynchronizationOffset().getString() << std::endl;
		return pStreamInf->getSynchronizationOffset();
	}

	pGroup->setStreamsChanged(false);

	MIPTime maxDiff = pGroup->calculateOffsets(); // this should take the old offsets into consideration!

	if (maxDiff > m_tolerance) 
	{
		// values changes too much, accept newly calculated offsets
		pGroup->acceptNewOffsets(); // this saves the new offsets as the old offsets
	}

//	std::cout << "Stream " << streamID << ": sync offset " << pStreamInf->getSynchronizationOffset().getString() << std::endl;
		
	return pStreamInf->getSynchronizationOffset();
}

