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
 * \file mipvideoframestorage.h
 */

#ifndef MIPVIDEOFRAMESTORAGE_H

#define MIPVIDEOFRAMESTORAGE_H

#include "mipconfig.h"
#include "mipcomponent.h"
#include "miptime.h"
#include <list>
#include <map>

/** A video frame storage component.
 *  This component accepts raw video frames in YUV420P format and stores the last frame
 *  received from each source. It does not produce any messages itself.
 */
class MIPVideoFrameStorage : public MIPComponent
{
public:
	MIPVideoFrameStorage();
	~MIPVideoFrameStorage();

	/** Initializes the component. */
	bool init();

	/** De-initializes the component. */
	bool destroy();
	
	/** Returns video frame data for source \c sourceID.
	 *  This function accesses the last video frame for a specific source.
	 *  \param sourceID The ID of the source whose video frame you which to access.
	 *  \param pDest Buffer to store the video frame data in, in YUV420P format. If the
	 *               parameter is NULL, the video frame is not copied. The buffer must
	 *               be large enough to store the frame in.
	 *  \param width If not NULL, the width of the video frame is stored in \c *width.
	 *  \param height If not NULL, the width of the video frame is stored in \c *height.
	 *  \param t If not NULL, the time at which the video frame was received is stored in \c *t.
	 */
	bool getData(uint64_t sourceID, uint8_t *pDest, int *width, int *height, MIPTime *t = 0);

	/** Fills in the list \c sourceIDs with the IDs of sources of which video frames are
	 *  currently being stored.
	 */
	bool getSourceIDs(std::list<uint64_t> &sourceIDs);

	bool push(const MIPComponentChain &chain, int64_t iteration, MIPMessage *pMsg);
	bool pull(const MIPComponentChain &chain, int64_t iteration, MIPMessage **pMsg);
private:
	class VideoFrameInfo
	{
	public:
		VideoFrameInfo(uint8_t *pData, int width, int height, size_t length)			{ m_pData = pData; m_length = length; m_width = width; m_height = height; m_time = MIPTime::getCurrentTime(); }
		~VideoFrameInfo()									{ delete [] m_pData; }
		const uint8_t *getData() const								{ return m_pData; }
		int getWidth() const									{ return m_width; }
		int getHeight() const									{ return m_height; }
		size_t getDataLength() const								{ return m_length; }
		MIPTime getCreationTime() const								{ return m_time; }
	private:
		uint8_t *m_pData;
		int m_width, m_height;
		size_t m_length;
		
		MIPTime m_time;
	};

	void expire();
	
	bool m_init;
	MIPTime m_lastExpireTime;
	std::map<uint64_t, VideoFrameInfo *> m_frames;
};

#endif // MIPVIDEOFRAMESTORAGE_H
