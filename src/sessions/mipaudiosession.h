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
 * \file mipaudiosession.h
 */

#ifndef MIPAUDIOSESSION_H

#define MIPAUDIOSESSION_H

#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_SPEEX) && ((defined(WIN32) || defined(_WIN32_WCE)) || \
		( !defined(WIN32) && !defined(_WIN32_WCE) && defined(MIPCONFIG_SUPPORT_OSS)))

#include "mipcomponentchain.h"
#include "miperrorbase.h"
#include "miptime.h"
#include "mipspeexencoder.h"
#include <rtptransmitter.h>
#include <string>

class RTPSession;
class RTPAddress;
class MIPRTPSynchronizer;
#if (defined(WIN32) || defined(_WIN32_WCE))
	class MIPWinMMInput;
	class MIPWinMMOutput;
#else
	class MIPOSSInputOutput;
#endif // WIN32 || _WIN32_WCE
class MIPAudioSplitter;
class MIPSampleEncoder;
class MIPSpeexEncoder;
class MIPRTPSpeexEncoder;
class MIPRTPComponent;
class MIPAverageTimer;
class MIPRTPDecoder;
class MIPRTPSpeexDecoder;
class MIPMediaBuffer;
class MIPSpeexDecoder;
class MIPSamplingRateConverter;
class MIPAudioMixer;

/** Parameters for an audio session. */
class MIPAudioSessionParams
{
public:
	MIPAudioSessionParams()								
	{ 
#if ! (defined(WIN32) || defined(_WIN32_WCE))
		m_inputDevName = std::string("/dev/dsp"); 
		m_outputDevName = std::string("/dev/dsp"); 
#endif // !(WIN32 || _WIN32_WCE)
		m_portbase = 5000; 
		m_acceptOwnPackets = false; 
		m_speexMode = MIPSpeexEncoder::WideBand;
	}
	~MIPAudioSessionParams()							{ }
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	/** Returns the name of the input device (not available on Win32/WinCE; default: /dev/dsp). */
	std::string getInputDevice() const						{ return m_inputDevName; }
	
	/** Returns the name of the output device (not available on Win32/WinCE; default: /dev/dsp). */
	std::string getOutputDevice() const						{ return m_outputDevName; }
#endif // !(WIN32 || _WIN32_WCE)
	/** Returns the RTP portbase (default: 5000). */
	uint16_t getPortbase() const							{ return m_portbase; }

	/** Returns \c true if own packets will be accepted (default: \c false). */
	bool getAcceptOwnPackets() const						{ return m_acceptOwnPackets; }
	
	/** Returns the Speex mode (default: WideBand). */
	MIPSpeexEncoder::SpeexBandWidth getSpeexEncoding() const			{ return m_speexMode; }
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	/** Sets the name of the input device (not available on Win32/WinCE). */
	void setInputDevice(const std::string &devName)					{ m_inputDevName = devName; }
	
	/** Sets the name of the input device (not available on Win32/WinCE). */
	void setOutputDevice(const std::string &devName)				{ m_outputDevName = devName; }
#endif // !(WIN32 || _WIN32_WCE)
	/** Sets the RTP portbase. */
	void setPortbase(uint16_t p)							{ m_portbase = p; }
	
	/** Sets a flag indicating if own packets should be accepted. */
	void setAcceptOwnPackets(bool v)						{ m_acceptOwnPackets = v; }

	/** Sets the Speex encoding mode. */
	void setSpeexEncoding(MIPSpeexEncoder::SpeexBandWidth b)			{ m_speexMode = b; }
private:
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	std::string m_inputDevName, m_outputDevName;
#endif // !(WIN32 || _WIN32_WCE)
	uint16_t m_portbase;
	bool m_acceptOwnPackets;
	MIPSpeexEncoder::SpeexBandWidth m_speexMode;
};

/** Creates a VoIP session.
 *  This wrapper class can be used to create a VoIP session. Transmitted data
 *  will be Speex encoded. Destinations are specified using subclasses of
 *  RTPAddress from the JRTPLIB library. Currently, only RTPIPv4Address
 *  instances can be specified.
 */
class MIPAudioSession : public MIPErrorBase
{
public:
	MIPAudioSession();
	virtual ~MIPAudioSession();

	/** Initializes the session.
	 *  Using this function, the session is initialized.
	 *  \param pParams Session parameters.
	 *  \param pSync RTP stream synchronizer.
	 */
	bool init(const MIPAudioSessionParams *pParams = 0, MIPRTPSynchronizer *pSync = 0);

	/** Destroys the session. */
	bool destroy();

	/** Add a destination. */
	bool addDestination(const RTPAddress &addr);

	/** Delete a destination. */
	bool deleteDestination(const RTPAddress &addr);

	/** Clear the destination list. */
	bool clearDestinations();

	/** Returns \c true if multicasting is supported, \c false otherwise. */
	bool supportsMulticasting();

	/** Joins a multicast group. */
	bool joinMulticastGroup(const RTPAddress &addr);

	/** Leaves a multicast group. */
	bool leaveMulticastGroup(const RTPAddress &addr);

	/** Leaves all multicast groups. */
	bool leaveAllMulticastGroups();
	
	/** Set a receive mode.
	 *  Using this function, a receive mode can be specified. Valid receive modes are
	 *  \c RTPTransmitter::AcceptAll, \c RTPTransmitter::AcceptSome and \c RTPTransmitter::IgnoreSome.
	 *  In the last two cases, packets are accepted or ignored based upon information in the
	 *  accept or ignore list. Note that changing the receive mode will cause such a list to be cleared.
	 */
	bool setReceiveMode(RTPTransmitter::ReceiveMode m);

	/** Adds an address to the ignore list. */
	bool addToIgnoreList(const RTPAddress &addr);

	/** Removes an address from the ignore list. */
	bool deleteFromIgnoreList(const RTPAddress &addr);

	/** Clears the ignore list. */
	bool clearIgnoreList();

	/** Adds an address to the accept list. */
	bool addToAcceptList(const RTPAddress &addr);

	/** Deletes an address from the accept list. */
	bool deleteFromAcceptList(const RTPAddress &addr);

	/** Clears the accept list. */
	bool clearAcceptList();
protected:
	/** Override this to use a user defined RTPSession object. */
	virtual RTPSession *newRTPSession()									{ return 0; }
	
	/** By overriding this function, you can detect when the input thread has finished.
	 *  By overriding this function, you can detect when the input thread has finished.
	 *  \param err Flag indicating if the thread stopped due to an error.
	 *  \param compName Contains the component in which the error occured.
	 *  \param errStr Contains a description of the error.
	 */
	virtual void onInputThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ }

	/** By overriding this function, you can detect when the output thread has finished.
	 *  By overriding this function, you can detect when the output thread has finished.
	 *  \param err Flag indicating if the thread stopped due to an error.
	 *  \param compName Contains the component in which the error occured.
	 *  \param errStr Contains a description of the error.
	 */
	virtual void onOutputThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ }

	/** By overriding this function, you can detect when the input/output thread has finished.
	 *  By overriding this function, you can detect when the input/output thread has finished.
	 *  A single input/output thread is used in the GNU/Linux version when input and output
	 *  OSS device are the same.
	 *  \param err Flag indicating if the thread stopped due to an error.
	 *  \param compName Contains the component in which the error occured.
	 *  \param errStr Contains a description of the error.
	 */
	virtual void onIOThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ }
private:
	class InputChain : public MIPComponentChain
	{
	public:
		InputChain(MIPAudioSession *pAudioSess) : MIPComponentChain("Input chain")		{ m_pAudioSess = pAudioSess; }
	protected:
		void onThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ m_pAudioSess->onInputThreadExit(err, compName, errStr); }
	private:
		MIPAudioSession *m_pAudioSess;
	};

	class OutputChain : public MIPComponentChain
	{
	public:
		OutputChain(MIPAudioSession *pAudioSess) : MIPComponentChain("Output chain")		{ m_pAudioSess = pAudioSess; }
	protected:
		void onThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ m_pAudioSess->onOutputThreadExit(err, compName, errStr); }
	private:
		MIPAudioSession *m_pAudioSess;
	};

	class IOChain : public MIPComponentChain
	{
	public:
		IOChain(MIPAudioSession *pAudioSess) : MIPComponentChain("Input/Output chain")		{ m_pAudioSess = pAudioSess; }
	protected:
		void onThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ m_pAudioSess->onIOThreadExit(err, compName, errStr); }
	private:
		MIPAudioSession *m_pAudioSess;
	};

	void zeroAll();
	void deleteAll();

	bool m_init;
	
	InputChain *m_pInputChain;
	OutputChain *m_pOutputChain;
	IOChain *m_pIOChain;
#if (defined(WIN32) || defined(_WIN32_WCE))
	MIPWinMMInput *m_pInput;
	MIPWinMMOutput *m_pOutput;
#else
	MIPOSSInputOutput *m_pInput;
	MIPOSSInputOutput *m_pOutput;
#endif // WIN32 || _WIN32_WCE
	MIPAudioSplitter *m_pSplitter;
	MIPSampleEncoder *m_pSampEnc;	
	MIPSpeexEncoder *m_pSpeexEnc;
	MIPRTPSpeexEncoder *m_pRTPEnc;
	MIPRTPComponent *m_pRTPComp;
	RTPSession *m_pRTPSession;
	MIPAverageTimer *m_pTimer;
	MIPRTPDecoder *m_pRTPDec;
	MIPRTPSpeexDecoder *m_pRTPSpeexDec;
	MIPMediaBuffer *m_pMediaBuf;
	MIPSpeexDecoder *m_pSpeexDec;
	MIPSamplingRateConverter *m_pSampConv;
	MIPAudioMixer *m_pMixer;
	MIPSampleEncoder *m_pSampEnc2;
	
	friend class InputChain;
	friend class OutputChain;
	friend class IOChain;
};

#endif // MIPCONFIG_SUPPORT_SPEEX && ((WIN32 || _WIN32_WCE) || (!WIN32 && !_WIN32_WCE && MIPCONFIG_SUPPORT_OSS))

#endif // MIPAUDIOSESSION_H

