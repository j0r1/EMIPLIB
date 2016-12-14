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
 * \file miperrorbase.h
 */

#ifndef MIPERRORBASE_H

#define MIPERRORBASE_H

#include "mipconfig.h"
#include <string>

/** This class provides the error description functions for other classes.
 */
class MIPErrorBase
{
public:
	MIPErrorBase()										{ }
	virtual ~MIPErrorBase()									{ }
	
	/** Returns the last known error description.
	 *  Typically, when some boolean function returned false, an error occured. A
	 *  string describing this error can then be retrieved by this function if the
	 *  derived class used the MIPErrorBase::setErrorString member function to set
	 *  a description.
	 */
	std::string getErrorString() const							{ return m_errStr; }
protected:
	/** Stores an error description.
	 *  If something goes wrong in a derived class it should store a description of what went
	 *  wrong. This which can be done using this function.
	 */
	void setErrorString(const std::string &str) const					{ m_errStr = str; }
private:
	mutable std::string m_errStr;
};

#endif // MIPERRORBASE_H
