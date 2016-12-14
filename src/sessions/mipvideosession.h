/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006  Hasselt University - Expertise Centre for
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
 * \file mipvideosession.h
 */

#ifndef MIPVIDEOSESSION_H

#define MIPVIDEOSESSION_H

#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_AVCODEC) && (defined(MIPCONFIG_SUPPORT_DIRECTSHOW) || defined(MIPCONFIG_SUPPORT_VIDEO4LINUX))

#include "mipcomponentchain.h"
#include "miperrorbase.h"
#include "miptime.h"
#include <rtptransmitter.h>
#include <string>

class RTPSession;
class RTPAddress;
class MIPRTPSynchronizer;
#if (defined(WIN32) || defined(_WIN32_WCE))
	class MIPDirectShowCapture;
#else
	class MIPV4LInput;
#endif // WIN32 || _WIN32_WCE
class MIPAVCodecEncoder;
class MIPRTPVideoEncoder;
class MIPRTPComponent;
class MIPAverageTimer;
class MIPRTPDecoder;
class MIPRTPVideoDecoder;
class MIPMediaBuffer;
class MIPAVCodecDecoder;
class MIPVideoMixer;
class MIPQtOutput;
class MIPVideoFrameStorage;

/** Parameters for a video session. */
class MIPVideoSessionParams
{
public:
	MIPVideoSessionParams()								
	{ 
#if (defined(WIN32) || defined(_WIN32_WCE))
		m_width = 160;
		m_height = 120;
		m_devNum = 0;
#else
		m_devName = std::string("/dev/video0"); 
#endif // !(WIN32 || _WIN32_WCE)
		m_frameRate = 15.0;
		m_portbase = 5100; 
		m_acceptOwnPackets = false;
		m_bandwidth = 200000;
		m_qtoutput = true;
	}
	~MIPVideoSessionParams()							{ }
#if (defined(WIN32) || defined(_WIN32_WCE))
	/** Returns the width (only available on Win32; default: 160). */
	int getWidth() const								{ return m_width; }
	
	/** Returns the height (only available on Win32; default: 120). */
	int getHeight() const								{ return m_height; }

	/** Returns the device number to open (only available on Win32; default: 0). */
	int getDevice() const								{ return m_devNum; }
#else
	/** Returns the device name (not available on Win32; default: /dev/video0). */
	std::string getDevice() const							{ return m_devName; }
#endif // (WIN32 || _WIN32_WCE)

	/** Returns the frame rate (default: 15.0). */
	real_t getFrameRate() const							{ return m_frameRate; }

	/** Returns the RTP portbase (default: 5100). */
	uint16_t getPortbase() const							{ return m_portbase; }

	/** Returns \c true if own packets will be accepted (default: \c false). */
	bool getAcceptOwnPackets() const						{ return m_acceptOwnPackets; }

	/** Returns the bandwidth that can be used (default: 200000). */
	int getBandwidth() const							{ return m_bandwidth; }
	
	/** Returns flag indicating if a Qt output component will be used or if a
	 *  video frame storage component will be used (default: \c true).
	 */
	bool getUseQtOutput() const							{ return m_qtoutput; }
#if (defined(WIN32) || defined(_WIN32_WCE))
	/** Sets the width of the video frames (only available on Win32). */
	void setWidth(int w)								{ m_width = w; }
	
	/** Sets the height of the video frames (only available on Win32). */
	void setHeight(int h)								{ m_height = h; }

	/** Sets the number of the input device to use (only available on Win32). */
	void setDevice(int n)								{ m_devNum = n; }
#else
	/** Sets the name of the device (not available on Win32). */
	void setDevice(const std::string &devName)					{ m_devName = devName; }
#endif // !(WIN32 || _WIN32_WCE)
	/** Sets the bandwidth. */
	void setBandwidth(int b)							{ m_bandwidth = b; }

	/** Sets the frame rate. */
	void setFrameRate(real_t r)							{ m_frameRate = r; }

	/** Sets the RTP portbase. */
	void setPortbase(uint16_t p)							{ m_portbase = p; }
	
	/** Sets a flag indicating if own packets should be accepted. */
	void setAcceptOwnPackets(bool v)						{ m_acceptOwnPackets = v; }

	/** Sets flag if Qt output component should be used or a if video frame
	 *  storage component should be used.
	 */
	void setUseQtOutput(bool f)							{ m_qtoutput = f; }
private:
#if (defined(WIN32) || defined(_WIN32_WCE))
	int m_width, m_height;
	int m_devNum;
#else
	std::string m_devName;
#endif // !(WIN32 || _WIN32_WCE)
	real_t m_frameRate;
	uint16_t m_portbase;
	bool m_acceptOwnPackets;
	int m_bandwidth;
	bool m_qtoutput;
};

/** Creates a video over IP session.
 *  This wrapper class can be used to create a video over IP session. Transmitted data
 *  will be H.263+ encoded. Destinations are specified using subclasses of
 *  RTPAddress from the JRTPLIB library. Currently, only RTPIPv4Address
 *  instances can be specified.
 */
class MIPVideoSession : public MIPErrorBase
{
public:
	MIPVideoSession();
	virtual ~MIPVideoSession();

	/** Initializes the session.
	 *  Using this function, the session is initialized.
	 *  \param pParams Session parameters.
	 *  \param pSync RTP stream synchronizer.
	 */
	bool init(const MIPVideoSessionParams *pParams = 0, MIPRTPSynchronizer *pSync = 0);
	
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

	/** If the video frame storage component is being used, this function retrieves the
	 *  list of source IDs of which video frames are currently stored.
	 */
	bool getSourceIDs(std::list<uint64_t> &sourceIDs);

	/** If the video frame storage component is being used, this function retrieves the last
	 *  received video frame of a specific source.
	 *  If the video frame storage component is being used, this function retrieves the last
	 *  received video frame of the source corresponding to \c sourceID.
	 *  \param sourceID The last frame of this source will be retrieved.
	 *  \param pData Will contain the video frame in YUV420P format. If the content of this
	 *               pointer is NULL, that the video frame is not more recent than the time 
	 *               specified in \c minimalTime.
	 *  \param pWidth Here, the width of the video frame is stored.
	 *  \param pHeight Used to store the height of the frame.
	 *  \param minimalTime Specifies that frame data should only be returned if the associated
	 *                     time is more recent than \c minimalTime.
	 */
	bool getVideoFrame(uint64_t sourceID, uint8_t **pData, int *pWidth, int *pHeight, MIPTime minimalTime = MIPTime(0));
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
private:
	class InputChain : public MIPComponentChain
	{
	public:
		InputChain(MIPVideoSession *pVideoSess) : MIPComponentChain("Input chain")		{ m_pVideoSess = pVideoSess; }
	protected:
		void onThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ m_pVideoSess->onInputThreadExit(err, compName, errStr); }
	private:
		MIPVideoSession *m_pVideoSess;
	};

	class OutputChain : public MIPComponentChain
	{
	public:
		OutputChain(MIPVideoSession *pVideoSess) : MIPComponentChain("Output chain")		{ m_pVideoSess = pVideoSess; }
	protected:
		void onThreadExit(bool err, const std::string &compName, const std::string &errStr)	{ m_pVideoSess->onOutputThreadExit(err, compName, errStr); }
	private:
		MIPVideoSession *m_pVideoSess;
	};

	void zeroAll();
	void deleteAll();

	bool m_init;
	
	InputChain *m_pInputChain;
	OutputChain *m_pOutputChain;
	MIPAverageTimer *m_pTimer;
#if (defined(WIN32) || defined(_WIN32_WCE))
	MIPDirectShowCapture *m_pInput;
#else
	MIPV4LInput *m_pInput;
#endif // WIN32 || _WIN32_WCE
	MIPAVCodecEncoder *m_pAvcEnc;
	MIPRTPVideoEncoder *m_pRTPEnc;
	MIPRTPComponent *m_pRTPComp;
	RTPSession *m_pRTPSession;
	MIPAverageTimer *m_pTimer2;
	MIPRTPDecoder *m_pRTPDec;
	MIPRTPVideoDecoder *m_pRTPVideoDec;
	MIPMediaBuffer *m_pMediaBuf;
	MIPAVCodecDecoder *m_pAvcDec;
	MIPVideoMixer *m_pMixer;
	MIPQtOutput *m_pQtOutput;
	MIPVideoFrameStorage *m_pStorage;
	
	friend class InputChain;
	friend class OutputChain;
};

#endif // MIPCONFIG_SUPPORT_AVCODEC && (MIPCONFIG_SUPPORT_DIRECTSHOW || MIPCONFIG_SUPPORT_VIDEO4LINUX)

#endif // MIPVIDEOSESSION_H

