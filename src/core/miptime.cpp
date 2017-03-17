/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2016  Hasselt University - Expertise Centre for
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
#define __STDC_FORMAT_MACROS
#include "mipconfig.h"
#include "miptime.h"
#include "mipcompat.h"
#include <inttypes.h>
#include <string>

std::string MIPTime::getString() const
{
	char str[256];

	MIP_SNPRINTF(str, 255, "%" PRId64 ".%06d", getSeconds(), (int)getMicroSeconds());
	
	return std::string(str);
}


