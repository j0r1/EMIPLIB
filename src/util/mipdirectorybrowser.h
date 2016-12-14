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
 * \file mipdirectorybrowser.h
 */

#ifndef MIPDIRECTORYBROWSER_H

#define MIPDIRECTORYBROWSER_H

#include "mipconfig.h"
#include "miperrorbase.h"
#if (defined(WIN32) || defined (_WIN32_WCE))
	#include <windows.h>
#else
	#include <sys/types.h>
	#include <dirent.h>
#endif // WIN32 || _WIN32_WCE
#include <string>

/** An object of this type can be used to get the file names in a
 *  specific directory.
 */
class EMIPLIB_IMPORTEXPORT MIPDirectoryBrowser : public MIPErrorBase
{
public:
	MIPDirectoryBrowser();
	~MIPDirectoryBrowser();

	/** Opens a directory. */
	bool open(const std::string &dirName);
	
	/** Retrieves the next entry in the directory. */
	bool getNextEntry(std::string &fileName, bool *isDirectory);

	/** Closes the directory. */
	bool close();
private:
#if (defined(WIN32) || defined (_WIN32_WCE))
	HANDLE m_findHandle;
	std::string m_nextFileName;
	bool m_gotNextFileName;
	bool m_isNextFileDirectory;
#else
	DIR *m_dir;
	std::string m_dirName;
#endif // WIN32 || _WIN32_WCE
};

#endif // MIPDIRECTORYBROWSER_H

