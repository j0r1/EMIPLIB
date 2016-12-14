/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2007  Hasselt University - Expertise Centre for
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
 * \file mipaudiosession.h
 */

#ifndef MIPAUDIOSESSION_H

#define MIPAUDIOSESSION_H

#include "mipconfig.h"

#if ((defined(WIN32) || defined(_WIN32_WCE)) || \
		( !defined(WIN32) && !defined(_WIN32_WCE) && (defined(MIPCONFIG_SUPPORT_OSS) || defined(MIPCONFIG_SUPPORT_PORTAUDIO) )))

#include "mipcomponentchain.h"
#include "miperrorbase.h"
#include "miptime.h"
#include <rtptransmitter.h>
#include <string>
#include <list>

class RTPSession;
class RTPAddress;
class MIPComponent;
class MIPRTPComponent;
class MIPRTPSynchronizer;
class MIPRTPPacketDecoder;

/** Parameters for an audio session. */
class MIPAudioSessionParams
{
public:
	/** Used to select compression/encoding type. */
	enum CompressionType 
	{ 
		/** U-law encoding. */
		ULaw, 
		/** A-law encoding. */
		ALaw,
		/** LPC compression. */
		LPC, 
		/** GSM 06.10 compression. */
		GSM, 
		/** Speex compression. */
		Speex,
		/** L16 mono. */
		L16Mono
	};

	/** If Speex compression is used, this is sed to select speex encoding type. */
	enum SpeexBandWidth 
	{ 
 		/** Narrow band mode (8000 Hz) */
		NarrowBand,
	 	/** Wide band mode (16000 Hz) */
		WideBand,		
 		/** Ultra wide band mode (32000 Hz) */
		UltraWideBand
	};
	
	MIPAudioSessionParams()								
	{ 
#if ! (defined(WIN32) || defined(_WIN32_WCE))
		m_inputDevName = std::string("/dev/dsp"); 
		m_outputDevName = std::string("/dev/dsp"); 
#else
		m_highPriority = false;
#endif // !(WIN32 || _WIN32_WCE)
		m_portbase = 5000; 
		m_acceptOwnPackets = false; 
		m_speexMode = WideBand;
		m_speexIncomingPT = 96;
		m_speexOutgoingPT = 96;
#ifdef _WIN32_WCE
		m_inputMultiplier = 3;
		m_outputMultiplier = 2;
#else
		m_inputMultiplier = 1;
		m_outputMultiplier = 1;
#endif // _WIN32_WCE
		m_compType = ULaw;
		m_disableInterChainTimer = false;
	}
	~MIPAudioSessionParams()							{ }
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	/** Returns the name of the input device (not available on Win32/WinCE; default: /dev/dsp). */
	std::string getInputDevice() const						{ return m_inputDevName; }
	
	/** Returns the name of the output device (not available on Win32/WinCE; default: /dev/dsp). */
	std::string getOutputDevice() const						{ return m_outputDevName; }
#else
	/** Returns \c true if the audio threads will receive high priority (only on Win32/WinCE; default: false). */
	bool getUseHighPriority() const							{ return m_highPriority; }
#endif // !(WIN32 || _WIN32_WCE)
	/** Returns the RTP portbase (default: 5000). */
	uint16_t getPortbase() const							{ return m_portbase; }

	/** Returns \c true if own packets will be accepted (default: \c false). */
	bool getAcceptOwnPackets() const						{ return m_acceptOwnPackets; }
	
	/** Returns the requested compression/encoding type (default: U-law). */
	CompressionType getCompressionType() const					{ return m_compType; }
	
	/** Returns the Speex mode (default: WideBand). */
	SpeexBandWidth getSpeexEncoding() const						{ return m_speexMode; }

	/** Incoming packets with this payload type will be interpreted as Speex packets. */
	uint8_t getSpeexIncomingPayloadType() const					{ return m_speexIncomingPT; }

	/** This payload type will be set on outgoing Speex packets. */
	uint8_t getSpeexOutgoingPayloadType() const					{ return m_speexOutgoingPT; }
	
	/** Returns the size of the input sampling interval, in units of 20ms. */
	int getInputMultiplier() const							{ return m_inputMultiplier; }

	/** Returns the size of the output sampling interval, in units of 20ms. */
	int getOutputMultiplier() const							{ return m_outputMultiplier; }

	/** Returns a flag indicating if inter-chain timing is disabled.
	 *  Returns a flag indicating if inter-chain timing is disabled. See
	 *  setDisableInterChainTimer for more information.
	 */
	bool getDisableInterChainTimer() const						{ return m_disableInterChainTimer; }
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	/** Sets the name of the input device (not available on Win32/WinCE). */
	void setInputDevice(const std::string &devName)					{ m_inputDevName = devName; }
	
	/** Sets the name of the input device (not available on Win32/WinCE). */
	void setOutputDevice(const std::string &devName)				{ m_outputDevName = devName; }
#else
	/** Sets a flag indicating if high priority audio threads should be used (only on Win32/WinCE). */
	void setUseHighPriority(bool f)							{ m_highPriority = f; }
#endif // !(WIN32 || _WIN32_WCE)
	/** Sets the RTP portbase. */
	void setPortbase(uint16_t p)							{ m_portbase = p; }
	
	/** Sets a flag indicating if own packets should be accepted. */
	void setAcceptOwnPackets(bool v)						{ m_acceptOwnPackets = v; }

	/** Sets the compression/encoding type. */
	void setCompressionType(CompressionType t)					{ m_compType = t; }

	/** Sets the Speex encoding mode. */
	void setSpeexEncoding(SpeexBandWidth b)						{ m_speexMode = b; }

	/** This will interpret incoming packets with payload type \c pt as Speex packets. */
	void setSpeexIncomingPayloadType(uint8_t pt)					{ m_speexIncomingPT = pt; }
	
	/** Sets the payload type for outgoing Speex packets. */
	void setSpeexOutgoingPayloadType(uint8_t pt)					{ m_speexOutgoingPT = pt; }

	/** Sets the input sampling interval to \c m * 20ms. */
	void setInputMultiplier(int m)							{ m_inputMultiplier = m; }

	/** Sets the output sampling interval to \c m * 20ms. */
	void setOutputMultiplier(int m)							{ m_outputMultiplier = m; }

	/** This can be used to always disable inter-chain timing.
	 *  This can be used to always disable inter-chain timing. If two chains
	 *  are used, one for recording/transmission and another for reception/playback,
	 *  it may be possible to use the timing component of the recording chain to
	 *  drive the playback chain. By default, this is turned on (if possible). Using
	 *  this flag, it can be turned off entirely, and a simple timing component
	 *  will then control the output chain. 
	 */
	void setDisableInterChainTimer(bool f)						{ m_disableInterChainTimer = f; }
private:
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	std::string m_inputDevName, m_outputDevName;
#else
	bool m_highPriority;
#endif // !(WIN32 || _WIN32_WCE)
	uint16_t m_portbase;
	bool m_acceptOwnPackets;
	SpeexBandWidth m_speexMode;
	int m_inputMultiplier, m_outputMultiplier;
	CompressionType m_compType;
	uint8_t m_speexOutgoingPT, m_speexIncomingPT;
	bool m_disableInterChainTimer;
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
	 *  \param pRTPSession Supply your own RTPSession instance with this parameter. In this case,
	 *                     the RTPSession instance is not deleted when the audio session is destroyed.
	 *                     The session has to be initialized, but the timestamp unit will still be 
	 *                     adjusted.
	 */
	bool init(const MIPAudioSessionParams *pParams = 0, MIPRTPSynchronizer *pSync = 0, RTPSession *pRTPSession = 0);

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
	void storeComponent(MIPComponent *pComp);
	void storePacketDecoder(MIPRTPPacketDecoder *pDec);
	void addLink(MIPComponentChain *pChain, MIPComponent **pPrevComp, MIPComponent *pComp, 
	             bool feedback = false, uint32_t mask1 = MIPMESSAGE_TYPE_ALL, uint32_t mask2 = MIPMESSAGE_TYPE_ALL);

	bool m_init;
	
	InputChain *m_pInputChain;
	OutputChain *m_pOutputChain;
	IOChain *m_pIOChain;
	
	MIPRTPComponent *m_pRTPComp;
	RTPSession *m_pRTPSession;
	bool m_deleteRTPSession;
	std::list<MIPComponent *> m_components;
	std::list<MIPRTPPacketDecoder *> m_packetDecoders;
	
	friend class InputChain;
	friend class OutputChain;
	friend class IOChain;
};

#endif // MIPCONFIG_SUPPORT_SPEEX && ((WIN32 || _WIN32_WCE) || (!WIN32 && !_WIN32_WCE && MIPCONFIG_SUPPORT_OSS))

#endif // MIPAUDIOSESSION_H

