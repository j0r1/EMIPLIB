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
 * \file mipqtoutput.h
 */

#ifndef MIPQTOUTPUT_H

#define MIPQTOUTPUT_H

#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_QT)

#include "mipcomponent.h"
#include "miptypes.h"
#include <jthread/jthread.h>
#include <list>

class MIPQtOutputMainWindow;
class MIPQtOutputWidget;

/** A Qt-based video output component.
 *  This component is a Qt-based video output component. The component shows all incoming video
 *  frames: one window is created for each source. Input video frames can be in either YUV420P
 *  or 32-bit RGB format.
 */
class MIPQtOutput : public MIPComponent, private jthread::JThread
{
public:
	MIPQtOutput();
	~MIPQtOutput();

	/** Initializes the Qt video output component. */
	bool init();

	/** De-initializes the component. */
	bool shutdown();
	
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	void *Thread();
	
	jthread::JMutex m_lock;
	std::list<uint64_t> m_newIDs;
	std::list<MIPQtOutputWidget *> m_outputWidgets;
	bool m_stopWindow;
	
	friend class MIPQtOutputMainWindow;
};

#endif // MIPCONFIG_SUPPORT_QT

#endif // MIPQTOUTPUT_H

