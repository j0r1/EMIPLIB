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
 * \file mipaudio3dbase.h
 */

#ifndef MIPAUDIO3DBASE_H

#define MIPAUDIO3DBASE_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <string.h>
#if defined(WIN32) || defined(_WIN32_WCE)
	#include <hash_map>
#else
	#include <ext/hash_map>
#endif // Win32

#define MIPAUDIO3DBASE_CONST_PI	3.14159265

/** Base class for 3D audio components.
 *  This class provides some functions for 3D audio components. You can set the position of a source and
 *  your own position and orientation. The distance unit is assumed to be one meter, but if your application
 *  works on a different scale, a scale factor can be installed to compensate. Finally, the class provides
 *  an implementation of the convolution product.
 */
class MIPAudio3DBase : public MIPComponent
{
protected:
	/** Constructor to be used by derived classes. */
	MIPAudio3DBase(const std::string &compName);
public:
	~MIPAudio3DBase();

	/** Changes between left and right handed coordinate systems.
	 *  By default, a left handed OpenGL-like coordinate system is used. Using this
	 *  function you can change this behavior.
	 */
	void setRightHanded(bool f = true)							{ m_rightHanded = f; }
	
	/** Set the position of a particular source.
	 *  Using this function, the position of a particular source is stored in a table. Note that
	 *  entries older than one minute are deleted from the table, so the function should be called
	 *  on a regular basis, even if the source is not moving.
	 */
	bool setSourcePosition(uint64_t sourceID, real_t pos[3]);

	/** Store your own position and orientation.
	 *  Using this function, you can store information about your own position and orientation.
	 *  The \c frontDirection and \c upDirection vectors need not be orthogonal and need not
	 *  be normalized.
	 */
	bool setOwnPosition(real_t pos[3], real_t frontDirection[3], real_t upDirection[3]);

	/** Set a scale factor for calculated distances.
	 *  With this function, you can set a scale factor to convert distances in your application
	 *  to distances which can be interpreted in units of meters.
	 */
	void setDistanceFactor(real_t distFact)							{ m_distanceFactor = distFact; }
protected:
	/** Cleans the table of source positions, resets your own position, etc. */
	void cleanUp();

	/** Remove old entries from the source table.
	 *  Using this function, old entries can be removed from the source table. Only when one
	 *  minute has passed since the last check, a new check is performed.
	 */
	void expirePositionalInfo();

	/** Obtain relative positional information for a specific source.
	 *  Based on the position of the source with ID \c sourceID and your own positional information,
	 *  the azimuth, elevation and distance are calculated. The meaning of azimuth and elevation are
	 *  illustrated in the following figure:
	 *  \image html sound3d.png
	 */
	bool getPositionalInfo(uint64_t sourceID, real_t *azimuth, real_t *elevation, real_t *distance);

	/** Performs a convolution.
	 *  Using this function, a convolution product can be calculated to obtain a 3D effect.
	 *  \param pDestStereo An array in which the calculated stereo sound will be stored.
	 *  \param numDestFrames The number of frames which can fit in the destination array.
	 *  \param scale A scale factor with which the left and right filters should be multilied.
	 *  \param pSrcMono The mono input sound.
	 *  \param numSrcFrames The number of input frames.
	 *  \param pLeftFilter The filter data for the left channel.
	 *  \param numLeftFrames The number of frames in the left channel filter.
	 *  \param pRightFilter The filter data for the right channel.
	 *  \param numRightFrames The number of frames in the right channel filter.
	 */
	void convolve(float *pDestStereo, int numDestFrames, float scale, const float *pSrcMono, 
	              int numSrcFrames, const float *pLeftFilter, int numLeftFrames, 
		      const float *pRightFilter, int numRightFrames);
private:
	class PositionalInfo
	{
	public:
		PositionalInfo(real_t x = 0, real_t y = 0, real_t z = 0, MIPTime updateTime = MIPTime(0))
		{
			m_x = x;
			m_y = y;
			m_z = z;
			m_updateTime = updateTime;
		}
		real_t getX() const								{ return m_x; }
		real_t getY() const								{ return m_y; }
		real_t getZ() const								{ return m_z; }
		MIPTime getLastUpdateTime() const						{ return m_updateTime; }
	private:
		real_t m_x, m_y, m_z;
		MIPTime m_updateTime;
	};

#if defined(WIN32) || defined(_WIN32_WCE)
	stdext::hash_map<uint64_t, PositionalInfo> m_posInfo;
#else
	__gnu_cxx::hash_map<uint64_t, PositionalInfo, __gnu_cxx::hash<uint32_t> > m_posInfo;
#endif // Win32

	real_t m_ownPos[3];
	real_t m_xDir[3]; // front direction
	real_t m_yDir[3]; // left ear direction
	real_t m_zDir[3]; // above head direction
	real_t m_distanceFactor;
	bool m_rightHanded;

	MIPTime m_lastExpireTime;
};

#ifndef MIPCONFIG_SUPPORT_INTELIPP

inline void MIPAudio3DBase::convolve(float *pDestStereo, int numDestFrames, float scale,
				     const float *pSrcMono, int numSrcFrames, 
                                     const float *pLeftFilter, int numLeftFrames, 
                                     const float *pRightFilter, int numRightFrames)
{
	int i,j,k,l;

	memset(pDestStereo,0,numDestFrames*2*sizeof(float));		
		
	for (i = 0, j = 0 ; i < numSrcFrames ; i++, j+= 2)
	{
		float val = scale*pSrcMono[i];
		for (k = 0,l = 0 ; k < numLeftFrames ; k++, l += 2)
			pDestStereo[j+l] += val*pLeftFilter[k];
		for (k = 0,l = 0 ; k < numRightFrames ; k++, l += 2)
			pDestStereo[j+l+1] += val*pRightFilter[k];
	}
}

#else

#include "ipp.h"
#include "ipps.h"

inline void MIPAudio3DBase::convolve(float *pDestStereo, int numDestFrames, float scale,
				     const float *pSrcMono, int numSrcFrames, 
                                     const float *pLeftFilter, int numLeftFrames, 
                                     const float *pRightFilter, int numRightFrames)
{
	float tmp[16384];
	float tmpLeft[16384];
	float tmpRight[16384];

	if (numDestFrames > 16384 || numSrcFrames > 16384)
	{
		ippsZero_32f(pDestStereo,numDestFrames*2);
		return;
	}
	
	ippsMulC_32f(pSrcMono, scale, tmp, numSrcFrames);
	ippsConv_32f(tmp, numSrcFrames, pLeftFilter, numLeftFrames, tmpLeft);
	ippsZero_32f(tmpLeft+numSrcFrames+numLeftFrames-1, (numDestFrames-(numSrcFrames+numLeftFrames-1)));
	ippsConv_32f(tmp, numSrcFrames, pRightFilter, numRightFrames, tmpRight);
	ippsZero_32f(tmpRight+numSrcFrames+numRightFrames-1, (numDestFrames-(numSrcFrames+numRightFrames-1)));
	for (int i = 0, j = 0 ; i < numDestFrames ; i++, j += 2)
	{
		pDestStereo[j] = tmpLeft[i];
		pDestStereo[j+1] = tmpRight[i];
	}
}

#endif // MIPCONFIG_SUPPORT_INTELIPP

#endif // MIPAUDIO3DBASE_H

