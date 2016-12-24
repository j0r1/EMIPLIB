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

#include "mipconfig.h"
#include "mipdirectorybrowser.h"

#include "mipdebug.h"

#define MIPDIRECTORYBROWSER_ERRSTR_NOTOPENED				"No directory was opened"
#define MIPDIRECTORYBROWSER_ERRSTR_ALREADYOPENED			"A directory was already opened"
#define MIPDIRECTORYBROWSER_ERRSTR_CANTSTAT				"Can't access specified path info"
#define MIPDIRECTORYBROWSER_ERRSTR_NODIRECTORY				"Specified path is not a directory"
#define MIPDIRECTORYBROWSER_ERRSTR_CANTOPENDIRECTORY			"Can't open the directory"

#if (defined(WIN32) || defined(_WIN32_WCE))

MIPDirectoryBrowser::MIPDirectoryBrowser()
{
	m_findHandle = INVALID_HANDLE_VALUE;
}

MIPDirectoryBrowser::~MIPDirectoryBrowser()
{
	close();
}

bool MIPDirectoryBrowser::open(const std::string &dirName)
{
	if (m_findHandle != INVALID_HANDLE_VALUE)
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_ALREADYOPENED);
		return false;
	}

	std::string searchString = dirName+std::string("\\*");
	WIN32_FIND_DATAA findData;
	
	m_findHandle = FindFirstFileA(searchString.c_str(),&findData);
	if (m_findHandle == INVALID_HANDLE_VALUE)
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_CANTOPENDIRECTORY);
		return false;
	}

	// ok, directory opened, save data of the first file
	
	m_gotNextFileName = true;
	m_nextFileName = std::string(findData.cFileName);
	if ((findData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		m_isNextFileDirectory = true;
	else
		m_isNextFileDirectory = false;

	return true;
}

bool MIPDirectoryBrowser::close()
{
	if (m_findHandle == INVALID_HANDLE_VALUE)
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_NOTOPENED);
		return false;
	}

	FindClose(m_findHandle);
	m_findHandle = INVALID_HANDLE_VALUE;
	return true;
}

bool MIPDirectoryBrowser::getNextEntry(std::string &fileName, bool *isDirectory)
{
	if (m_findHandle == INVALID_HANDLE_VALUE)
		return false;

	if (!m_gotNextFileName)
		return false;

	fileName = m_nextFileName;
	*isDirectory = m_isNextFileDirectory;

	WIN32_FIND_DATAA findData;
	
	if (FindNextFileA(m_findHandle, &findData))
	{
		m_gotNextFileName = true;
		m_nextFileName = std::string(findData.cFileName);
		if ((findData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			m_isNextFileDirectory = true;
		else
			m_isNextFileDirectory = false;
	}
	else
		m_gotNextFileName = false;

	return true;
}
		
#else // unix

#include <sys/stat.h>
#include <unistd.h>

MIPDirectoryBrowser::MIPDirectoryBrowser()
{
	m_dir = 0;
}

MIPDirectoryBrowser::~MIPDirectoryBrowser()
{
	close();
}

bool MIPDirectoryBrowser::open(const std::string &dirName)
{
	if (m_dir != 0)
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_ALREADYOPENED);
		return false;
	}
	
	struct stat statBuf;
	
	if (stat(dirName.c_str(),&statBuf) == -1)
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_CANTSTAT);
		return false;
	}

	if (!S_ISDIR(statBuf.st_mode))
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_NODIRECTORY);
		return false;
	}

	m_dir = opendir(dirName.c_str());
	if (m_dir == 0)
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_CANTOPENDIRECTORY);
		return false;
	}

	m_dirName = dirName;
	return true;
}

bool MIPDirectoryBrowser::close()
{
	if (m_dir != 0)
	{
		setErrorString(MIPDIRECTORYBROWSER_ERRSTR_NOTOPENED);
		return false;
	}

	closedir(m_dir);
	m_dir = 0;
	return true;
}

bool MIPDirectoryBrowser::getNextEntry(std::string &fileName, bool *isDirectory)
{
	if (m_dir == 0)
		return false;

	struct dirent *entry = readdir(m_dir);
	
	if (entry == 0)
		return false;

	fileName = std::string(entry->d_name);
	
	struct stat statBuf;
	std::string fullPath = m_dirName + std::string("/") + fileName;
	
	*isDirectory = false;
	
	if (stat(fullPath.c_str(),&statBuf) == 0)
	{
		if (S_ISDIR(statBuf.st_mode))
			*isDirectory = true;
	}

	return true;
}
		
#endif // WIN32 || _WIN32_WCE

