/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2010  Hasselt University - Expertise Centre for
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
 * \file mipcomponentalias.h
 */

#ifndef MIPCOMPONENTALIAS_H

#define MIPCOMPONENTALIAS_H

#include "mipconfig.h"
#include "mipcomponent.h"

/** This class provides a wrapper around another component, essentially
 *  providing a different pointer to the same object.
 *  This class provides a wrapper around another component, essentially
 *  providing a different pointer to the same object.
 *
 * In the documentation on the main page, the following example was used
 * to illustrate how different branches in a chain may merge again:
 *
 * \image html subchains1.png
 *
 * It was also explained why the following chain was not a good choice,
 * as it can cause some synchronization problems:
 * \image html subchains3.png
 *
 * The basic problem was that in this case the lengths of the branches
 * are not equal, which can cause a packet being extracted from the audio
 * mixer \em before the sampling rate converter passes another packet to
 * this mixer. To solve this, either the first chain shown above can be
 * used, or a MIPComponentAlias can be used:
 *
 * \image html subchains2.png
 *
 * In this chain, the alias is simply a wrapper around the previous
 * soundfile input component, and the dotted connection indicates
 * that we won't really be passing any messages between the component
 * and its alias (so we won't send messages from the component to
 * itself).
 * The link from the component alias to the mixer will then effectively
 * pass messages from the soundfile input component to the mixer.
 * The subchains now have an equal length, avoiding the synchronization
 * problem mentioned earlier.
 *
 * Here is some code which illustrates how this is done, the full
 * source code can be found in \c examples/multiplesoundfileplayer.cpp , in which
 * the effect of the other two chains can be heard as well. First,
 * we'll define the components in the chain:
 *
 * \code
 * 	MIPTime interval(1.000); // one second intervals
 * 	MIPAverageTimer timer(interval);
 * 	MIPWAVInput sndFileInput2;
 * 	MIPComponentAlias alias(&sndFileInput2);
 * 	MIPWAVInput sndFileInput1;
 * 	MIPSamplingRateConverter sampConv;
 * 	MIPAudioMixer mixer;
 * 	MIPSampleEncoder sampEnc;
 * #ifndef WIN32
 * 	MIPOSSInputOutput sndCardOutput;
 * #else
 * 	MIPWinMMOutput sndCardOutput;
 * #endif
 * \endcode
 *
 * As you can see, we have specified that a component named 'alias' will
 * actually refer to one of the soundfile input components. Then, we
 * shall build the chain by specifying the start component and the
 * connections in the chain:
 *
 * \code
 * 	returnValue = chain.setChainStart(&timer);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&timer, &sndFileInput1);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&timer, &sndFileInput2);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sndFileInput1, &sampConv);
 * 	checkError(returnValue, chain);
 *
 * 	returnValue = chain.addConnection(&sndFileInput2, &alias, false, 0, 0);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sampConv, &mixer);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&alias, &mixer);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&mixer, &sampEnc);
 * 	checkError(returnValue, chain);
 *
 * 	returnValue = chain.addConnection(&sampEnc, &sndCardOutput);
 * 	checkError(returnValue, chain);
 * \endcode
 *
 * Note that in specifying the connection from the soundfile input
 * component to its alias, the last two parameters of MIPComponentChain::addConnection
 * have been set to zero. This will make sure that no messages are passed
 * from the soundfile component to its alias.
 *
 * Now you may wonder why a construction with an alias is necessary,
 * why it's not possible to simply use the component itself. Suppose
 * we would not use the alias, and we would use the following
 * connections:
 *
 * \code
 * 	returnValue = chain.addConnection(&timer, &sndFileInput2);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sndFileInput2, &sndFileInput2, false, 0, 0);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sndFileInput2, &mixer);
 * 	checkError(returnValue, chain);
 * \endcode
 *
 * In that case we would not create subchains of equal length, but
 * effectively the following situation would be created: 
 *
 * \image html subchains4.png
 *
 * In this chain, it is of course still possible that the same synchronization
 * problem occurs.
 */
class MIPComponentAlias : public MIPComponent
{
public:
	/** Create a component alias for component \c pComp. */
	MIPComponentAlias(MIPComponent *pComp) : MIPComponent(pComp->getComponentName() + std::string("-alias")){ m_pComponent = pComp; }
	~MIPComponentAlias()											{ }

	void lock()												{ m_pComponent->lock(); MIPComponent::lock(); }
	void unlock()												{ m_pComponent->unlock(); MIPComponent::unlock(); }
	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)				{ bool status = m_pComponent->push(chain, iteration, pMsg); if (!status) setErrorString(m_pComponent->getErrorString()); return status; }
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)				{ bool status = m_pComponent->pull(chain, iteration, pMsg); if (!status) setErrorString(m_pComponent->getErrorString()); return status; }
	bool processFeedback(const MIPComponentChain &chain, int64_t feedbackChainID, MIPFeedback *feedback)	{ bool status = m_pComponent->processFeedback(chain, feedbackChainID, feedback); if (!status) setErrorString(m_pComponent->getErrorString()); return status; }

	const MIPComponent *getComponentPointer() const								{ return m_pComponent; }
private:
	MIPComponent *m_pComponent;
};

#endif // MIPCOMPONENTALIAS_H

