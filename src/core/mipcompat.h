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

#ifndef MIPCOMPAT_H

#define MIPCOMPAT_H

#include "mipconfig.h"

#if (defined(WIN32) || defined(_WIN32_WCE))
	#if (!defined(_WIN32_WCE)) && (defined(_MSC_VER) && _MSC_VER >= 1400 )
		#define MIP_SNPRINTF _snprintf_s
		#define MIP_SSCANF sscanf_s
	#else
		#define MIP_SNPRINTF _snprintf
		#define MIP_SSCANF sscanf
	#endif
#else
	#define MIP_SNPRINTF snprintf
	#define MIP_SSCANF sscanf
#endif // WIN32 || _WIN32_WCE

#endif // MIPCOMPAT_H

