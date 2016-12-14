#include "mipconfig.h"
#include "mipaudiodistancefade.h"
#include "miprawaudiomessage.h"

#define MIPAUDIODISTANCEFADE_ERRSTR_ALREADYINIT			"Already initialized"
#define MIPAUDIODISTANCEFADE_ERRSTR_NOTINIT			"Not initialized"
#define MIPAUDIODISTANCEFADE_ERRSTR_BADMESSAGE			"Not a floating point raw audio message"

MIPAudioDistanceFade::MIPAudioDistanceFade() : MIPAudio3DBase("MIPAudioDistanceFade")
{
	m_init = false;
}

MIPAudioDistanceFade::~MIPAudioDistanceFade()
{
	destroy();
}
	
bool MIPAudioDistanceFade::init(real_t cutOffDistance)
{
	if (m_init)
	{
		setErrorString(MIPAUDIODISTANCEFADE_ERRSTR_ALREADYINIT);
		return false;
	}
	
	m_msgIt = m_messages.begin();
	m_cutOffDistance = cutOffDistance;
	m_curIteration = -1;
	m_init = true;

	return true;
}

bool MIPAudioDistanceFade::destroy()
{
	if (!m_init)
	{
		setErrorString(MIPAUDIODISTANCEFADE_ERRSTR_NOTINIT);
		return false;
	}

	cleanUp();

	std::list<MIPRawFloatAudioMessage *>::iterator it;

	for (it = m_messages.begin() ; it != m_messages.end() ; it++)
		delete (*it);
	m_messages.clear();

	m_msgIt = m_messages.begin();
	m_init = false;
	
	return true;
}
	
bool MIPAudioDistanceFade::push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIODISTANCEFADE_ERRSTR_NOTINIT);
		return false;
	}

	if (iteration != m_curIteration)
	{
		std::list<MIPRawFloatAudioMessage *>::iterator it;

		for (it = m_messages.begin() ; it != m_messages.end() ; it++)
			delete (*it);
		m_messages.clear();

		m_msgIt = m_messages.begin();

		m_curIteration = iteration;
	}
	
	expirePositionalInfo();

	if (!(pMsg->getMessageType() == MIPMESSAGE_TYPE_AUDIO_RAW && pMsg->getMessageSubtype() == MIPRAWAUDIOMESSAGE_TYPE_FLOAT))
	{
		setErrorString(MIPAUDIODISTANCEFADE_ERRSTR_BADMESSAGE);
		return false;
	}

	
	MIPRawFloatAudioMessage *pFloatMsg = (MIPRawFloatAudioMessage *)pMsg;
	uint64_t sourceID = pFloatMsg->getSourceID();
	real_t azimuth, elevation, distance;

	if (!getPositionalInfo(sourceID, &azimuth, &elevation, &distance))
	{
		// No source with this ID found, ignore it
		return true;
	}
	
	if (distance > m_cutOffDistance)
	{
		// Source is too far away, ignore it
		return true;
	}

	int numSamples = pFloatMsg->getNumberOfFrames()*pFloatMsg->getNumberOfChannels();
	const float *pOldSamples = pFloatMsg->getFrames();
	float *pNewSamples = new float[numSamples];
	float scaleFactor = (float)(1.0/(1.0+distance));

	for (int i = 0 ; i < numSamples ; i++)
		pNewSamples[i] = pOldSamples[i]*scaleFactor;

	MIPRawFloatAudioMessage *pNewMsg = new MIPRawFloatAudioMessage(pFloatMsg->getSamplingRate(), pFloatMsg->getNumberOfChannels(), pFloatMsg->getNumberOfFrames(), pNewSamples, true);
	pNewMsg->copyMediaInfoFrom(*pFloatMsg);

	m_messages.push_back(pNewMsg);
	m_msgIt = m_messages.begin();
	
	return true;
}

bool MIPAudioDistanceFade::pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg)
{
	if (!m_init)
	{
		setErrorString(MIPAUDIODISTANCEFADE_ERRSTR_NOTINIT);
		return false;
	}
	
	if (iteration != m_curIteration)
	{
		std::list<MIPRawFloatAudioMessage *>::iterator it;

		for (it = m_messages.begin() ; it != m_messages.end() ; it++)
			delete (*it);
		m_messages.clear();

		m_msgIt = m_messages.begin();

		m_curIteration = iteration;
	}

	if (m_msgIt == m_messages.end())
	{
		m_msgIt = m_messages.begin();
		*pMsg = 0;
	}
	else
	{
		*pMsg = (*m_msgIt);
		m_msgIt++;
	}
	return true;
}

