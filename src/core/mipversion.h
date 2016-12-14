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
 * \file mipversion.h
 */

#ifndef MIPVERSION_H

#define MIPVERSION_H

#include "mipconfig.h"
#include <string>

/** Version information about the library. */ 
class MIPVersion
{
public:
	MIPVersion();
	~MIPVersion();

	/** Returns the major version number. */
	int getMajorVersion() const								{ return m_major; }
	
	/** Returns the minor version number. */
	int getMinorVersion() const								{ return m_minor; }
	
	/** Returns the debug version number. */
	int getDebugVersion() const								{ return m_debug; }

	/** Returns the version string. */
	std::string getVersionString() const;

	/** Returns the license which applies to this library. */
	std::string getLicense() const								{ return m_license; }
private:
	std::string m_license;
	int m_major, m_minor, m_debug;
};

#endif // MIPVERSION_H

