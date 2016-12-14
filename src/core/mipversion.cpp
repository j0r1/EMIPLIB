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

#include "mipconfig.h"
#include "mipversion.h"
#include "mipcompat.h"

MIPVersion::MIPVersion()
{
	m_major = 0;
	m_minor = 12;
	m_debug = 0;
#ifdef MIPCONFIG_GPL
	m_license = std::string("GPL");
#else
	m_license = std::string("LGPL");
#endif // MIPCONFIG_GPL
}

MIPVersion::~MIPVersion()
{
}

std::string MIPVersion::getVersionString() const
{
	char str[16];
	
	MIP_SNPRINTF(str, 16, "%d.%d.%d", m_major, m_minor, m_debug);

	return std::string(str);
}

