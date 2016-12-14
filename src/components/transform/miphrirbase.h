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

/**
 * \file miphrirbase.h
 */

#ifndef MIPHRIRBASE_H

#define MIPHRIRBASE_H

#include "mipconfig.h"
#include "mipaudio3dbase.h"
#include <cmath>
#include <list>

/** Base class for HRIR based 3D audio components. */
class MIPHRIRBase : public MIPAudio3DBase
{
protected:
	/** Constructor to be used by derived classes. */
	MIPHRIRBase(const std::string &compName) : MIPAudio3DBase(compName) 			{ }
public:
	~MIPHRIRBase()										{ }
protected:
	class HRIRData
	{
	public:
		HRIRData(real_t radius, int azimuth, int elevation, float *pLeftChannel, int numLeft,
		         float *pRightChannel, int numRight)
		{
			m_radius = radius;
			m_azimuth = azimuth;
			m_elevation = elevation;
			m_pLeftChannel = pLeftChannel;
			m_pRightChannel = pRightChannel;
			m_numLeft = numLeft;
			m_numRight = numRight;
			m_azimuthRad = (((real_t)azimuth)/180.0)*MIPAUDIO3DBASE_CONST_PI;
			m_elevationRad = (((real_t)elevation)/180.0)*MIPAUDIO3DBASE_CONST_PI;
		}
		~HRIRData()
		{
			delete [] m_pLeftChannel;
			delete [] m_pRightChannel;
		}

		real_t getRadius() const							{ return m_radius; }
		int getAzimuth() const								{ return m_azimuth; }
		int getElevation() const							{ return m_elevation; }
		real_t getAzimuthRadians() const						{ return m_azimuthRad; }
		real_t getElevationRadians() const						{ return m_elevationRad; }
		float *getLeftChannel() const							{ return m_pLeftChannel; }
		float *getRightChannel() const							{ return m_pRightChannel; }
		int getNumberOfLeftSamples() const						{ return m_numLeft; }
		int getNumberOfRightSamples() const						{ return m_numRight; }
	private:
		real_t m_radius, m_azimuthRad, m_elevationRad;
		int m_azimuth, m_elevation;
		int m_numLeft, m_numRight;
		float *m_pLeftChannel, *m_pRightChannel;
	};
	
	class HRIRInfo
	{
	public:
		HRIRInfo(int subjectNumber)							{ m_subjectNumber = subjectNumber; }
		~HRIRInfo()
		{
			std::list<HRIRData *>::iterator it;

			for (it = m_HRIRSet.begin() ; it != m_HRIRSet.end() ; it++)
				delete (*it);
		}
		
		int getSubjectNumber() const							{ return m_subjectNumber; }

		// radius in meters, azimuth in degrees, elevation in degrees
		bool addHRIRData(real_t radius, int azimuth, int elevation, float *pLeftChannel, 
				 int numLeft, float *pRightChannel, int numRight)
		{
			std::list<HRIRData *>::const_iterator it;
			bool found = false;
			
			for (it = m_HRIRSet.begin() ; !found && it != m_HRIRSet.end() ; it++)
			{
				if (azimuth == (*it)->getAzimuth() && elevation == (*it)->getElevation())
					found = true;
			}

			if (found)
				return false;
			
			m_HRIRSet.push_back(new HRIRData(radius, azimuth, elevation, pLeftChannel, numLeft, pRightChannel, numRight));
			return true;
		}

		// both azimuth and elevation in radians
		HRIRData *findBestMatch(real_t azimuth, real_t elevation)
		{
			std::list<HRIRData *>::const_iterator it;
			real_t minDist = 2.0*MIPAUDIO3DBASE_CONST_PI;
			HRIRData *bestData = 0;
			
			for (it = m_HRIRSet.begin() ; it != m_HRIRSet.end() ; it++)
			{
				real_t A2 = (*it)->getAzimuthRadians();
				real_t E2 = (*it)->getElevationRadians();

				real_t b = MIPAUDIO3DBASE_CONST_PI/2.0-elevation;
				real_t c = MIPAUDIO3DBASE_CONST_PI/2.0-E2;
					
				real_t cosDist = cos(b)*cos(c) + sin(b)*sin(c)*cos(A2-azimuth);
				real_t dist = acos(cosDist);

				if (dist < minDist)
				{
					minDist = dist;
					bestData = (*it);
				}
			}
			return bestData;
		}
	private:
		int m_subjectNumber;
		std::list<HRIRData *> m_HRIRSet;
	};
};

#endif // MIPHRIRBASE_H

