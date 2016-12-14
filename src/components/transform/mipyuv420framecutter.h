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
 * \file mipyuv420framecutter.h
 */

#ifndef MIPYUV420FRAMECUTTER_H

#define MIPYUV420FRAMECUTTER_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>

class MIPVideoMessage;

/** Extracts a part of an YUV420P frame.
 *  This component takes a raw video frame, YUV420P encoded, as input and
 *  creates a video frame (same encoding) which containts a part of the 
 *  input frame as output.
 */
class EMIPLIB_IMPORTEXPORT MIPYUV420FrameCutter : public MIPComponent
{
public:
	MIPYUV420FrameCutter();
	~MIPYUV420FrameCutter();

	/** Initializes the component.
	 *  Initializes the component.
	 *  \param inputWidth The width that each input frame must have.
	 *  \param inputHeight The height that each input frame must have.
	 *  \param x0 Start X coordinate of the subframe.
	 *  \param x1 End X coordinate (not included) of the subframe.
	 *  \param y0 Start Y coordinate of the subframe.
	 *  \param y1 End Y coordinate (not included) of the subframe.
	 *  The subframe will have (x1-x0) as its width and (y1-y0) as
	 *  its height.
	 */
	bool init(int inputWidth, int inputHeight,
		  int x0, int x1, int y0, int y1);

	/** Deinitializes the component. */
	bool destroy();

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void clearMessages();

	bool m_init;
	std::list<MIPVideoMessage *> m_messages;
	std::list<MIPVideoMessage *>::const_iterator m_msgIt;
	int64_t m_lastIteration;

	int m_x0, m_x1, m_y0, m_y1;
	int m_inputWidth, m_inputHeight;
};

#endif // MIPYUV420FRAMECUTTER_H

