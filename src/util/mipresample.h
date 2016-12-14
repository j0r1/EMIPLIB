/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Expertise Centre for Digital Media (EDM)
                      (http://www.edm.uhasselt.be)

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
 * \file mipresample.h
 */

#ifndef MIPRESAMPLE_H

#define MIPRESAMPLE_H

#include "mipconfig.h"
#include <stdio.h>

#define MIPRESAMPLE_MAXCHANNELS									16

/** A template function for resampling audio data.
 *  This template function allows you to resample audio data. Input and output samples are
 *  of type \c Tio, internal calculations will be done using samples of type \c Tcalc.
 */
template<class Tio, class Tcalc>
bool MIPResample(const Tio *pInputFrames, int numInputFrames, int numInputChannels,
		 Tio *pOutputFrames, int numOutputFrames, int numOutputChannels)
{
	if (numInputFrames < 1 || numOutputFrames < 1)
		return false;
	if (numInputChannels < 1 || numOutputChannels < 1)
		return false;
	if (numInputChannels > 1)
	{
		if (numInputChannels != numOutputChannels && numOutputChannels != 1)
			return false;
	}
	if (numOutputChannels > 1)
	{
		if (numInputChannels != numOutputChannels && numInputChannels != 1)
			return false;
	}
	
	if (numInputChannels > MIPRESAMPLE_MAXCHANNELS || numOutputChannels > MIPRESAMPLE_MAXCHANNELS)
		return false;

	if (numInputFrames > numOutputFrames)
	{
		const Tio *pIn = pInputFrames;
		Tio *pOut = pOutputFrames;

		for (int i = 0 ; i < numOutputFrames ; i++, pOut += numOutputChannels)
		{
			Tcalc inputSum[MIPRESAMPLE_MAXCHANNELS];
				
			for (int j = 0 ; j < numInputChannels ; j++)
				inputSum[j] = 0;

			int startFrame = (i*numInputFrames)/numOutputFrames;
			int stopFrame = ((i+1)*numInputFrames)/numOutputFrames;
			int num = stopFrame-startFrame;
			
			for (int j = 0 ; j < num ; j++, pIn += numInputChannels)
			{
				for (int k = 0 ; k < numInputChannels ; k++)
					inputSum[k] += (Tcalc)pIn[k];
			}

			for (int j = 0 ; j < numInputChannels ; j++)
				inputSum[j] /= (Tcalc)num;

			if (numInputChannels > 1 && numOutputChannels == 1)
			{
				Tcalc sum = 0;

				for (int j = 0 ; j < numInputChannels ; j++)
					sum += inputSum[j];
				sum /= (Tcalc)numInputChannels;
				pOut[0] = (Tio)sum;
			}
			else if (numInputChannels == 1 && numOutputChannels > 1)
			{
				Tio val = (Tio)inputSum[0];
				
				for (int j = 0 ; j < numOutputChannels ; j++)
					pOut[j] = val;
			}
			else // numInputChannels == numOutputChannels
			{
				for (int j = 0 ; j < numOutputChannels ; j++)
					pOut[j] = (Tio)inputSum[j];
			}
		}
	}
	else if (numInputFrames < numOutputFrames)
	{
		const Tio *pIn = pInputFrames;
		Tio *pOut = pOutputFrames;
		Tcalc stepValues[MIPRESAMPLE_MAXCHANNELS];

		for (int j = 0 ; j < numInputChannels ; j++)
			stepValues[j] = 0;

		for (int i = 0 ; i < numInputFrames ; i++, pIn += numInputChannels)
		{
			int startFrame = (i*numOutputFrames)/numInputFrames;
			int stopFrame = ((i+1)*numOutputFrames)/numInputFrames;
			int num = stopFrame-startFrame;
			Tcalc startValues[MIPRESAMPLE_MAXCHANNELS];

			for (int j = 0 ; j < numInputChannels ; j++)
				startValues[j] = (Tcalc)pIn[j];
			if (i < numInputFrames - 1)
			{
				for (int j = 0 ; j < numInputChannels ; j++)
					stepValues[j] = (Tcalc)pIn[j+numInputChannels] - startValues[j];
			}

			for (int k = 0 ; k < num ; k++, pOut += numOutputChannels)
			{
				Tcalc interpolation[MIPRESAMPLE_MAXCHANNELS];

				for (int j = 0 ; j < numInputChannels ; j++)
					interpolation[j] = startValues[j]+((Tcalc)j)*stepValues[j]/(Tcalc)num;

				if (numInputChannels > 1 && numOutputChannels == 1)
				{
					Tcalc sum = 0;

					for (int j = 0 ; j < numInputChannels ; j++)
						sum += interpolation[j];
					sum /= (Tcalc)numInputChannels;
					pOut[0] = (Tio)sum;
				}
				else if (numInputChannels == 1 && numOutputChannels > 1)
				{
					Tio val = (Tio)interpolation[0];
					
					for (int j = 0 ; j < numOutputChannels ; j++)
						pOut[j] = val;
				}
				else // numInputChannels == numOutputChannels
				{
					for (int j = 0 ; j < numOutputChannels ; j++)
						pOut[j] = (Tio)interpolation[j];
				}
			}
		}
	}
	else // numInputFrames == numOutputFrames
	{
		const Tio *pIn = pInputFrames;
		Tio *pOut = pOutputFrames;
		Tcalc inputSum[MIPRESAMPLE_MAXCHANNELS];
		
		for (int i = 0 ; i < numOutputFrames ; i++, pOut += numOutputChannels, pIn += numInputChannels)
		{
			for (int k = 0 ; k < numInputChannels ; k++)
				inputSum[k] = (Tcalc)pIn[k];

			if (numInputChannels > 1 && numOutputChannels == 1)
			{
				Tcalc sum = 0;

				for (int j = 0 ; j < numInputChannels ; j++)
					sum += inputSum[j];
				sum /= (Tcalc)numInputChannels;
				pOut[0] = (Tio)sum;
			}
			else if (numInputChannels == 1 && numOutputChannels > 1)
			{
				Tio val = (Tio)inputSum[0];
				
				for (int j = 0 ; j < numOutputChannels ; j++)
					pOut[j] = val;
			}
			else // numInputChannels == numOutputChannels
			{
				for (int j = 0 ; j < numOutputChannels ; j++)
					pOut[j] = (Tio)inputSum[j];
			}

		}
	}

	return true;
}

#endif // MIPRESAMPLE_H

