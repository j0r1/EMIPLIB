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
 * \file mipvideosession.h
 */

#ifndef MIPVIDEOSESSION_H

#define MIPVIDEOSESSION_H

#include "mipconfig.h"

#if defined(MIPCONFIG_SUPPORT_AVCODEC) && (defined(MIPCONFIG_SUPPORT_DIRECTSHOW) || defined(MIPCONFIG_SUPPORT_VIDEO4LINUX))

#include "mipcomponentchain.h"
#include "miperrorbase.h"
#include "miptime.h"
#include <jrtplib3/rtptransmitter.h>
#include <string>

namespace jrtplib
{
	class RTPSession;
	class RTPAddress;
}

class MIPRTPSynchronizer;
class MIPDirectShowCapture;
class MIPV4L2Input;
class MIPAVCodecEncoder;
class MIPRTPH263Encoder;
class MIPRTPVideoEncoder;
class MIPRTPComponent;
class MIPAverageTimer;
class MIPRTPDecoder;
class MIPRTPH263Decoder;
class MIPRTPVideoDecoder;
class MIPRTPDummyDecoder;
class MIPMediaBuffer;
class MIPAVCodecDecoder;
class MIPVideoMixer;
class MIPQt5OutputComponent;
class MIPVideoFrameStorage;
class MIPTinyJPEGDecoder;
class MIPAVCodecFrameConverter;
class MIPComponentAlias;
class QWidget;

/** Parameters for a video session. */
class EMIPLIB_IMPORTEXPORT MIPVideoSessionParams
{
public:
	/** Specifies the type of session that will be used. */
	enum SessionType 
	{ 
		InputOutput, 	/**< Both video capture and playback will take place. */
		OutputOnly 	/**< Received video will be shown, but no frames will be captured locally. */
	};
	
	/** Specifies the encoding/compression of outgoing video frames. */
	enum EncodingType 
	{ 
		H263, 		/**< H.263 compression is used, in an RTP format based on RFC 4629. */
		IntH263, 	/**< H.263 compression is used, but stored in RTP packets using an internal format, not compatible with other software. */
		IntYUV420 	/**< Raw YUV420P frames will be sent, using an internal format for storage into RTP packets. */
	};

	MIPVideoSessionParams()								
	{ 
		m_devNum = 0;
		m_devName = std::string("/dev/video0"); 
		m_width = 160;
		m_height = 120;
		m_frameRate = 15.0;
		m_portbase = 5100; 
		m_acceptOwnPackets = false;
		m_bandwidth = 200000;
		m_qtoutput = true;
		m_type = InputOutput;
		m_outH263PayloadType = 34;
		m_inH263PayloadType = 34;
		m_outIntPayloadType = 103;
		m_inIntPayloadType = 103;
		m_encType = H263;
		m_waitForKeyframe = true;
		m_maxPayloadSize = 64000;
	}
	~MIPVideoSessionParams()							{ }

	/** Returns the device number to open (only used on Win32; default: 0). */
	int getDeviceNumber() const							{ return m_devNum; }

	/** Returns the device name (not used on Win32; default: /dev/video0). */
	std::string getDeviceName() const						{ return m_devName; }

	/** Returns the width (default: 160). */
	int getWidth() const								{ return m_width; }
	
	/** Returns the height (default: 120). */
	int getHeight() const								{ return m_height; }

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

	/** Returns the type of session that will be created (default: MIPVideoSessionParams::InputOutput). */
	SessionType getSessionType() const						{ return m_type; }

	/** Returns the payload type that should used to interpret incoming packets
	 *  as H.263 encoded video, stored in RTP packets in the standard way (default: 34).
	 */
	uint8_t getIncomingH263PayloadType() const					{ return m_inH263PayloadType; }

	/** Returns the payload type that will be set on outgoing RTP packets if they
	 *  contain H.263 video stored in the standard way (default: 34).
	 */
	uint8_t getOutgoingH263PayloadType() const					{ return m_outH263PayloadType; }

	/** Returns the payload type that should used to interpret incoming packets
	 *  as video packets using an internal encoding format (default: 103).
	 */
	uint8_t getIncomingInternalPayloadType() const					{ return m_inIntPayloadType; }

	/** Returns the payload type that will be set on outgoing RTP packets if they
	 *  contain video stored in the non-standard, internal format (default: 103).
	 */
	uint8_t getOutgoingInternalPayloadType() const					{ return m_outIntPayloadType; }

	/** Returns the encoding that outgoing video frames will have 
	 *  (default: MIPVideoSessionParams::H263).
	 */
	EncodingType getEncodingType() const						{ return m_encType; }

	/** Returns \c true if video frames should only be shown after a keyframe
	 *  has been received (default: \c true).
	 */
	bool getWaitForKeyframe() const							{ return m_waitForKeyframe; }

	/** Returns the maximum payload size a single RTP packet may have, before
	 *  splitting data over multiple packets (default: 64000).
	 */
	int getMaximumPayloadSize() const						{ return m_maxPayloadSize; }

	/** Sets the number of the input device to use (only used on Win32). */
	void setDevice(int n)								{ m_devNum = n; }

	/** Sets the name of the device (not used on Win32). */
	void setDevice(const std::string &devName)					{ m_devName = devName; }

	/** Sets the width of the video frames. */
	void setWidth(int w)								{ m_width = w; }
	
	/** Sets the height of the video frames. */
	void setHeight(int h)								{ m_height = h; }

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

	/** Sets the current session type. */
	void setSessionType(SessionType s)						{ m_type = s; }

	/** Sets the payload type that will be used to interpret RTP packets as
	 *  standard H.263 encoded video.
	 */
	void setIncomingH263PayloadType(uint8_t pt)					{ m_inH263PayloadType = pt; }

	/** Sets the payload type that will be used for outgoing messages when
	 *  sending H.263 data in the standard way.
	 */
	void setOutgoingH263PayloadType(uint8_t pt)					{ m_outH263PayloadType = pt; }

	/** Sets the payload type that will be used to interpret RTP packets as
	 *  video frames which use an internal RTP format.
	 */
	void setIncomingInternalPayloadType(uint8_t pt)					{ m_inIntPayloadType = pt; }

	/** Sets the payload type that will be used for outgoing messages when
	 *  sending video frames using the non-standard, internal payload format.
	 */
	void setOutgoingInternalPayloadType(uint8_t pt)					{ m_outIntPayloadType = pt; }

	/** Sets the current encoding type. */
	void setEncodingType(EncodingType t)						{ m_encType = t; }

	/** If set to \c true, video frames will only be shown after a keyframe
	 *  has been received.
	 */
	void setWaitForKeyframe(bool w)							{ m_waitForKeyframe = w; }

	/** Sets the maximum size the payload of an RTP packet may have before
	 *  splitting it over multiple RTP packets.
	 */
	void setMaximumPayloadSize(int s)						{ m_maxPayloadSize = s; }
private:
	int m_devNum;
	std::string m_devName;
	int m_width, m_height;
	real_t m_frameRate;
	uint16_t m_portbase;
	bool m_acceptOwnPackets;
	int m_bandwidth;
	bool m_qtoutput;
	SessionType m_type;
	uint8_t m_inH263PayloadType;
	uint8_t m_outH263PayloadType;
	uint8_t m_inIntPayloadType;
	uint8_t m_outIntPayloadType;
	EncodingType m_encType; 
	bool m_waitForKeyframe;
	int m_maxPayloadSize;
};

/** Creates a video over IP session.
 *  This wrapper class can be used to create a video over IP session. Transmitted data
 *  will be H.263+ encoded. Destinations are specified using subclasses of
 *  RTPAddress from the JRTPLIB library. Currently, only RTPIPv4Address
 *  instances can be specified.
 */
class EMIPLIB_IMPORTEXPORT MIPVideoSession : public MIPErrorBase
{
public:
	MIPVideoSession();
	virtual ~MIPVideoSession();

	/** Initializes the session.
	 *  Using this function, the session is initialized.
	 *  \param pParams Session parameters.
	 *  \param pSync RTP stream synchronizer.
	 *  \param pRTPSession Supply your own RTPSession instance with this parameter. In this case,
	 *                     the RTPSession instance is not deleted when the video session is destroyed.
	 *                     The session has to be initialized, but the timestamp unit will still be 
	 *                     adjusted.
	 *  \param autoStart If `true`, the constructed chain is started before the function returns. If
	 *                   `false` is specified instead, a call to MIPVideoSession::startChain is necessary.
 	 */
	bool init(const MIPVideoSessionParams *pParams = 0, MIPRTPSynchronizer *pSync = 0, jrtplib::RTPSession *pRTPSession = 0,
	          bool autoStart = true);
	
	/** Starts the component chain that was constructed in the MIPVideoSession::init function, and
	 *  is necessary if the `autoStart` parameter of that function was set to `false`. */
	bool startChain();

	/** Destroys the session. */
	bool destroy();

	/** Add a destination. */
	bool addDestination(const jrtplib::RTPAddress &addr);

	/** Delete a destination. */
	bool deleteDestination(const jrtplib::RTPAddress &addr);

	/** Clear the destination list. */
	bool clearDestinations();

	/** Returns \c true if multicasting is supported, \c false otherwise. */
	bool supportsMulticasting();

	/** Joins a multicast group. */
	bool joinMulticastGroup(const jrtplib::RTPAddress &addr);

	/** Leaves a multicast group. */
	bool leaveMulticastGroup(const jrtplib::RTPAddress &addr);

	/** Leaves all multicast groups. */
	bool leaveAllMulticastGroups();
	
	/** Set a receive mode.
	 *  Using this function, a receive mode can be specified. Valid receive modes are
	 *  \c RTPTransmitter::AcceptAll, \c RTPTransmitter::AcceptSome and \c RTPTransmitter::IgnoreSome.
	 *  In the last two cases, packets are accepted or ignored based upon information in the
	 *  accept or ignore list. Note that changing the receive mode will cause such a list to be cleared.
	 */
	bool setReceiveMode(jrtplib::RTPTransmitter::ReceiveMode m);

	/** Adds an address to the ignore list. */
	bool addToIgnoreList(const jrtplib::RTPAddress &addr);

	/** Removes an address from the ignore list. */
	bool deleteFromIgnoreList(const jrtplib::RTPAddress &addr);

	/** Clears the ignore list. */
	bool clearIgnoreList();

	/** Adds an address to the accept list. */
	bool addToAcceptList(const jrtplib::RTPAddress &addr);

	/** Deletes an address from the accept list. */
	bool deleteFromAcceptList(const jrtplib::RTPAddress &addr);

	/** Clears the accept list. */
	bool clearAcceptList();

	/** If the video frame storage component is being used, this function retrieves the
	 *  list of source IDs of which video frames are currently stored.
	 */
	bool getSourceIDs(std::list<uint64_t> &sourceIDs);

	/** If the Qt5 output component is being used, this retrieves the output
	 *  component, which can be used to render the incoming video frames (do _not_
	 *  delete this component yourself, it is managed internally). */
	MIPQt5OutputComponent *getQt5OutputComponent();

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
#ifdef MIPCONFIG_SUPPORT_DIRECTSHOW
	MIPDirectShowCapture *m_pInput;
#else
	MIPV4L2Input *m_pInput;
#endif // MIPCONFIG_SUPPORT_DIRECTSHOW
	MIPTinyJPEGDecoder *m_pTinyJpegDec;
	MIPAVCodecFrameConverter *m_pInputFrameConverter;
	MIPAVCodecFrameConverter *m_pOutputFrameConverter;
	MIPAVCodecEncoder *m_pAvcEnc;
	MIPRTPH263Encoder *m_pRTPH263Enc;
	MIPRTPVideoEncoder *m_pRTPIntVideoEnc;
	MIPRTPComponent *m_pRTPComp;
	
	jrtplib::RTPSession *m_pRTPSession;
	bool m_deleteRTPSession;
	
	MIPAverageTimer *m_pTimer2;
	MIPRTPDecoder *m_pRTPDec;
	MIPRTPH263Decoder *m_pRTPH263Dec;
	MIPRTPVideoDecoder *m_pRTPIntVideoDec;
	MIPRTPDummyDecoder *m_pRTPDummyDec;
	MIPMediaBuffer *m_pMediaBuf;
	MIPComponentAlias *m_pBufferAlias;
	MIPAVCodecDecoder *m_pAvcDec;
	MIPVideoMixer *m_pMixer;
	MIPQt5OutputComponent *m_pOutputComponent;
	MIPVideoFrameStorage *m_pStorage;
	
	friend class InputChain;
	friend class OutputChain;
};

#endif // MIPCONFIG_SUPPORT_AVCODEC && (MIPCONFIG_SUPPORT_DIRECTSHOW || MIPCONFIG_SUPPORT_VIDEO4LINUX)

#endif // MIPVIDEOSESSION_H

