/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
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
#include "mipmessagedumper.h"
#include "mipcomponentchain.h"
#include "miptime.h"
#include <iostream>
#include <stdio.h>

#include "mipdebug.h"

MIPMessageDumper::MIPMessageDumper() : MIPComponent("MIPMessageDumper")
{
}

MIPMessageDumper::~MIPMessageDumper()
{
}

bool MIPMessageDumper::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	MIPTime t = MIPTime::getCurrentTime();
	
	std::cout << "Time: " << t.getString() << std::endl;
	std::cout << "Chain: " << chain.getName() << std::endl;
	std::cout << "Iteration: " << iteration << std::endl;
	std::cout << "MIPMessage type: " << pMsg->getMessageType() << std::endl;
	std::cout << "MIPMessage subtype: " << pMsg->getMessageSubtype() << std::endl;
	std::cout << std::endl;
	return true;
}

bool MIPMessageDumper::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	setErrorString("Pull not implemented");
	return false;
}

