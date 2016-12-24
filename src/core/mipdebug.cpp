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

#ifdef MIPDEBUG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jthread/jmutex.h>
#include <jthread/jmutexautolock.h>

using namespace jthread;

struct MemoryInfo
{
	void *ptr;
	size_t size;
	int lineno;
	char *filename;
	
	MemoryInfo *next;
};

JMutex mutex;

class MemoryTracker
{
public:
	MemoryTracker() { firstblock = NULL; mutex.Init(); }
	~MemoryTracker()
	{
		JMutexAutoLock l(mutex);

		MemoryInfo *tmp;
		int count = 0;
		
		printf("Checking for memory leaks...\n");fflush(stdout);
		while(firstblock)
		{
			count++;
			printf("Unfreed block %p of %d bytes (file '%s', line %d)\n",firstblock->ptr,(int)firstblock->size,firstblock->filename,firstblock->lineno);;
			
			tmp = firstblock->next;
			
			free(firstblock->ptr);
			if (firstblock->filename)
				free(firstblock->filename);
			free(firstblock);
			firstblock = tmp;
		}
		if (count == 0)
			printf("No memory leaks found\n");
		else
			printf("%d leaks found\n",count);
	}
	
	MemoryInfo *firstblock;
};

static MemoryTracker memtrack;

void *donew(size_t s,char filename[],int line)
{	
	JMutexAutoLock l(mutex);
	
	void *p;
	MemoryInfo *meminf;
	
	p = malloc(s);
	meminf = (MemoryInfo *)malloc(sizeof(MemoryInfo));
	
	meminf->ptr = p;
	meminf->size = s;
	meminf->lineno = line;
	meminf->filename = (char *)malloc(strlen(filename)+1);
	strcpy(meminf->filename,filename);
	meminf->next = memtrack.firstblock;
	
	memtrack.firstblock = meminf;
	
	return p;
}

void dodelete(void *p)
{
	JMutexAutoLock l(mutex);

	MemoryInfo *tmp,*tmpprev;
	bool found;
	
	tmpprev = NULL;
	tmp = memtrack.firstblock;
	found = false;
	while (tmp != NULL && !found)
	{
		if (tmp->ptr == p)
			found = true;
		else
		{
			tmpprev = tmp;
			tmp = tmp->next;
		}
	}
	if (!found)
	{
		printf("Couldn't free block %p!\n",p);
		fflush(stdout);
	}
	else
	{
		MemoryInfo *n;
		
		fflush(stdout);
		n = tmp->next;
		free(tmp->ptr);
		if (tmp->filename)
			free(tmp->filename);
		free(tmp);
		
		if (tmpprev)
			tmpprev->next = n;
		else
			memtrack.firstblock = n;
	}
}

void *operator new(size_t s)
{
	return donew(s,"UNKNOWN FILE",0);
}

void *operator new[](size_t s)
{
	return donew(s,"UNKNOWN FILE",0);
}

void *operator new(size_t s,char filename[],int line)
{
	return donew(s,filename,line);
}

void *operator new[](size_t s,char filename[],int line)
{
	return donew(s,filename,line);
}

void operator delete(void *p)
{
	dodelete(p);
}

void operator delete[](void *p)
{
	dodelete(p);
}

#endif // MIPDEBUG

