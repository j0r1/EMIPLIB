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
#include "mipaudio3dbase.h"
#include <cmath>

#if defined(WIN32) || defined(_WIN32_WCE)
using namespace stdext;
#else
using namespace __gnu_cxx;
#endif // Win32

#define MIPAUDIO3DBASE_ERRSTR_CANTNORMALIZEFRONTDIRECTION	"Can't normalize front direction"
#define MIPAUDIO3DBASE_ERRSTR_CANTNORMALIZERIGHTDIRECTION	"Can't normalize right direction"

MIPAudio3DBase::MIPAudio3DBase(const std::string &compName) : MIPComponent(compName)
{
	cleanUp();
}

MIPAudio3DBase::~MIPAudio3DBase()
{
	cleanUp();
}

bool MIPAudio3DBase::setSourcePosition(uint64_t sourceID, real_t pos2[3])
{
#if (defined(WIN32) || defined(_WIN32_WCE))
	hash_map<uint64_t, PositionalInfo>::iterator it = m_posInfo.find(sourceID);
#else
	hash_map<uint64_t, PositionalInfo, hash<uint32_t> >::iterator it = m_posInfo.find(sourceID);
#endif // WIN32 || _WIN32_WCE
	MIPTime curTime = MIPTime::getCurrentTime();
	
	// Check coordinate system
	
	real_t pos[3];
	
	if (m_rightHanded)
	{
		pos[0] = pos2[0];
		pos[1] = pos2[1];
		pos[2] = pos2[2];
	}
	else // left handed system, switch Y and Z coordinates
	{	
		pos[0] = pos2[0];
		pos[2] = pos2[1];
		pos[1] = pos2[2];
	}
	
	if (it == m_posInfo.end()) // add new entry
	{
		m_posInfo[sourceID] = PositionalInfo(pos[0], pos[1], pos[2], curTime);
	}
	else // entry found
	{
		(*it).second = PositionalInfo(pos[0], pos[1], pos[2], curTime);
	}

	return true;
}

bool MIPAudio3DBase::setOwnPosition(real_t pos2[3], real_t frontDirection2[3], real_t upDirection2[3])
{
	// Check coordinate system
	
	real_t pos[3],frontDirection[3],upDirection[3];
	
	if (m_rightHanded)
	{
		pos[0] = pos2[0];
		pos[1] = pos2[1];
		pos[2] = pos2[2];
		frontDirection[0] = frontDirection2[0];
		frontDirection[1] = frontDirection2[1];
		frontDirection[2] = frontDirection2[2];
		upDirection[0] = upDirection2[0];
		upDirection[1] = upDirection2[1];
		upDirection[2] = upDirection2[2];
	}
	else // left handed system, switch Y and Z coordinates
	{	
		pos[0] = pos2[0];
		pos[2] = pos2[1];
		pos[1] = pos2[2];
		frontDirection[0] = frontDirection2[0];
		frontDirection[2] = frontDirection2[1];
		frontDirection[1] = frontDirection2[2];
		upDirection[0] = upDirection2[0];
		upDirection[2] = upDirection2[1];
		upDirection[1] = upDirection2[2];
	}
	
	// First, we normalize the frontDirection and the upDirection
	
	real_t lengthFront = sqrt(frontDirection[0]*frontDirection[0]+
			          frontDirection[1]*frontDirection[1]+
			          frontDirection[2]*frontDirection[2]);
	real_t frontDir[3];

	if (lengthFront == 0)
	{
		setErrorString(MIPAUDIO3DBASE_ERRSTR_CANTNORMALIZEFRONTDIRECTION);
		return false;
	}
	
	frontDir[0] = frontDirection[0]/lengthFront;
	frontDir[1] = frontDirection[1]/lengthFront;
	frontDir[2] = frontDirection[2]/lengthFront;

	real_t rightDir[3];

	rightDir[0] = frontDir[1]*upDirection[2]-frontDir[2]*upDirection[1];
	rightDir[1] = frontDir[2]*upDirection[0]-frontDir[0]*upDirection[2];
	rightDir[2] = frontDir[0]*upDirection[1]-frontDir[1]*upDirection[0];

	real_t lengthRight = sqrt(rightDir[0]*rightDir[0]+
			          rightDir[1]*rightDir[1]+
				  rightDir[2]*rightDir[2]);

	if (lengthRight == 0)
	{
		setErrorString(MIPAUDIO3DBASE_ERRSTR_CANTNORMALIZERIGHTDIRECTION);
		return false;
	}
	
	rightDir[0] /= lengthRight;
	rightDir[1] /= lengthRight;
	rightDir[2] /= lengthRight;

	real_t upDir[3];

	upDir[0] = -(frontDir[1]*rightDir[2]-frontDir[2]*rightDir[1]);
	upDir[1] = -(frontDir[2]*rightDir[0]-frontDir[0]*rightDir[2]);
	upDir[2] = -(frontDir[0]*rightDir[1]-frontDir[1]*rightDir[0]);

	// Ok, we now got an orthonormal coordinate system
	
	m_xDir[0] = frontDir[0];
	m_xDir[1] = frontDir[1];
	m_xDir[2] = frontDir[2];

	m_yDir[0] = -rightDir[0];
	m_yDir[1] = -rightDir[1];
	m_yDir[2] = -rightDir[2];

	m_zDir[0] = upDir[0];
	m_zDir[1] = upDir[1];
	m_zDir[2] = upDir[2];

	m_ownPos[0] = pos[0];
	m_ownPos[1] = pos[1];
	m_ownPos[2] = pos[2];
	
	return true;
}

void MIPAudio3DBase::cleanUp()
{
	m_ownPos[0] = 0;
	m_ownPos[1] = 0;
	m_ownPos[2] = 0;

	m_xDir[0] = 1;
	m_xDir[1] = 0;
	m_xDir[2] = 0;

	m_yDir[0] = 0;
	m_yDir[1] = 1;
	m_yDir[2] = 0;

	m_zDir[0] = 0;
	m_zDir[1] = 0;
	m_zDir[2] = 1;

	m_distanceFactor = 1.0;
	m_rightHanded = false;
	m_posInfo.clear();

	m_lastExpireTime = MIPTime::getCurrentTime();
}

void MIPAudio3DBase::expirePositionalInfo()
{
	MIPTime curTime = MIPTime::getCurrentTime();

	if (curTime.getValue() - m_lastExpireTime.getValue() < 60.0) // clean up source table every minute
		return;

#if (defined(WIN32) || defined(_WIN32_WCE))
	hash_map<uint64_t, PositionalInfo>::iterator it;
#else
	hash_map<uint64_t, PositionalInfo, hash<uint32_t> >::iterator it;
#endif // WIN32 || _WIN32_WCE
	
	it = m_posInfo.begin();
	while (it != m_posInfo.end())
	{
		if ((curTime.getValue() - (*it).second.getLastUpdateTime().getValue() ) < 60.0)
			it++;
		else
		{
#if (defined(WIN32) || defined(_WIN32_WCE))
			hash_map<uint64_t, PositionalInfo>::iterator it2;
#else
			hash_map<uint64_t, PositionalInfo, hash<uint32_t> >::iterator it2;
#endif // WIN32 || _WIN32_WCE

		//	std::cerr << "Cleaning  up: " << (*it).first << std::endl;
			it2 = it;
			it++;
			m_posInfo.erase(it2);
		}
	}
}

bool MIPAudio3DBase::getPositionalInfo(uint64_t sourceID, real_t *azimuth2, real_t *elevation2, real_t *distance2)
{
#if (defined(WIN32) || defined(_WIN32_WCE))
	hash_map<uint64_t, PositionalInfo>::iterator it = m_posInfo.find(sourceID);
#else
	hash_map<uint64_t, PositionalInfo, hash<uint32_t> >::iterator it = m_posInfo.find(sourceID);
#endif // WIN32 || _WIN32_WCE
	if (it == m_posInfo.end())
		return false;

	// first, determine azimuth and elevation
		
	real_t relativePosition[3];

	relativePosition[0] = (*it).second.getX() - m_ownPos[0];
	relativePosition[1] = (*it).second.getY() - m_ownPos[1];
	relativePosition[2] = (*it).second.getZ() - m_ownPos[2];

	real_t distance = sqrt(relativePosition[0]*relativePosition[0]+
			       relativePosition[1]*relativePosition[1]+
			       relativePosition[2]*relativePosition[2]);

	if (distance == 0)
	{
		*azimuth2 = 0;
		*elevation2 = 0;
		*distance2 = 0;
		return true;
	}
		
	real_t direction[3];

	direction[0] = relativePosition[0]/distance;
	direction[1] = relativePosition[1]/distance;
	direction[2] = relativePosition[2]/distance;
		
	real_t zcos = direction[0]*m_zDir[0] + direction[1]*m_zDir[1] + direction[2]*m_zDir[2];

	real_t elevation = MIPAUDIO3DBASE_CONST_PI/2.0-acos(zcos);
	real_t planeVector[3];
		
	planeVector[0] = direction[0] - zcos*m_zDir[0];
	planeVector[1] = direction[1] - zcos*m_zDir[1];
	planeVector[2] = direction[2] - zcos*m_zDir[2];

	real_t planeVectorLength = sqrt(planeVector[0]*planeVector[0] + planeVector[1]*planeVector[1] + planeVector[2]*planeVector[2]);

	if (planeVectorLength == 0)
	{
		*elevation2 = elevation;
		*azimuth2 = 0;
		*distance2 = distance*m_distanceFactor;
		return true;
	}

	real_t azimuth = acos(planeVector[0]*m_xDir[0] + planeVector[1]*m_xDir[1] + planeVector[2]*m_xDir[2]);
	real_t signTest = acos(planeVector[0]*m_yDir[0] + planeVector[1]*m_yDir[1] + planeVector[2]*m_yDir[2]);

	if (signTest > MIPAUDIO3DBASE_CONST_PI/2.0)
		azimuth = 2.0*MIPAUDIO3DBASE_CONST_PI - azimuth;
	
	// calculated everything, store values
	
	*azimuth2 = azimuth;
	*elevation2 = elevation;
	*distance2 = distance*m_distanceFactor;

	return true;
}

