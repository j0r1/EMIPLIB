#include "miprtppacketgrouper.h"
#include <jrtplib3/rtppacket.h>
#include <string.h>

#include "mipdebug.h"

using namespace jrtplib;

#define MIPRTPPACKETGROUPER_ERRSTR_BADSSRC		"SSRC of RTP packet doesn't match SSRC during initialization"
#define MIPRTPPACKETGROUPER_ERRSTR_BADBUFSIZE		"Minimal number of buffers should be 32"

#define MIPRTPPACKETGROUPER_MINBUFSIZE			32

MIPRTPPacketGrouper::MIPRTPPacketGrouper()
{
	m_startPosition = -1;
	m_ssrc = 0;
}

MIPRTPPacketGrouper::~MIPRTPPacketGrouper()
{
	clear();
}

bool MIPRTPPacketGrouper::init(uint32_t ssrc, int bufSize)
{
	if (bufSize < MIPRTPPACKETGROUPER_MINBUFSIZE)
	{
		setErrorString(MIPRTPPACKETGROUPER_ERRSTR_BADBUFSIZE);
		return false;
	}

	clear();

	m_ssrc = ssrc;
	m_extendedSequenceNumbers.resize(bufSize);
	m_timestamps.resize(bufSize);
	m_skipFlags.resize(bufSize);
	m_packetParts.resize(bufSize);
	m_partSizes.resize(bufSize);
	m_markers.resize(bufSize);
	m_firstPartMarkers.resize(bufSize);
	m_receiveTimes.resize(bufSize);

	m_startPosition = -1;
	m_ssrc = ssrc;

	return true;
}

void MIPRTPPacketGrouper::clear()
{
	m_extendedSequenceNumbers.resize(0);
	m_timestamps.resize(0);
	m_skipFlags.resize(0);

	for (size_t i = 0 ; i < m_packetParts.size() ; i++)
		delete [] m_packetParts[i];

	m_packetParts.resize(0);
	m_markers.resize(0);
	m_partSizes.resize(0);
	m_firstPartMarkers.resize(0);
	m_receiveTimes.resize(0);

	m_startPosition = -1;
	m_ssrc = 0;
}

bool MIPRTPPacketGrouper::processPacket(const RTPPacket *pPack, bool isFirstFramePart)
{
	if (m_ssrc != pPack->GetSSRC())
	{
		setErrorString(MIPRTPPACKETGROUPER_ERRSTR_BADSSRC);
		return false;
	}

	real_t receiveTime = pPack->GetReceiveTime().GetDouble();
	uint32_t seqNr = pPack->GetExtendedSequenceNumber();
	size_t bufSize = m_extendedSequenceNumbers.size();
	bool veryFirstPacket = false;

	if (m_startPosition == -1) // first packet
	{
		m_startPosition = 0;

		for (size_t i = 0 ; i < bufSize ; i++, seqNr++)
		{
			m_extendedSequenceNumbers[i] = seqNr;
			m_timestamps[i] = 0;
			m_skipFlags[i] = false;
			m_packetParts[i] = 0;
			m_partSizes[i] = 0;
			m_markers[i] = false;
			m_firstPartMarkers[i] = false;
			m_receiveTimes[i] = 0;
		}

		// store the current sequence number again
		seqNr = pPack->GetExtendedSequenceNumber();
		veryFirstPacket = true;
	}

	int foundIdx = -1;

	/*
	
	// naive routine
	
	for (int i = 0 ; foundIdx == -1 && i < bufSize ; i++)
	{
		if (m_extendedSequenceNumbers[i] == seqNr)
			foundIdx = i;
	}
	*/

	uint32_t offset = seqNr - m_extendedSequenceNumbers[m_startPosition];

	if (offset < 0x80000000) // positive number
	{
		int pos = (int)( ((uint32_t)m_startPosition + offset) % (uint32_t)bufSize );

		if (m_extendedSequenceNumbers[pos] == seqNr)
			foundIdx = pos;
	}

	if (foundIdx != -1)
	{
		if (m_skipFlags[foundIdx] == false && m_packetParts[foundIdx] == 0)
		{
			size_t packLen = pPack->GetPayloadLength();
			uint32_t timestamp = pPack->GetTimestamp();
			
			m_partSizes[foundIdx] = packLen;
			m_timestamps[foundIdx] = timestamp;
			m_packetParts[foundIdx] = new uint8_t[packLen];
			m_markers[foundIdx] = pPack->HasMarker();
			m_firstPartMarkers[foundIdx] = isFirstFramePart;
			m_receiveTimes[foundIdx] = receiveTime;

			memcpy(m_packetParts[foundIdx], pPack->GetPayloadData(), packLen);

			// check if this packet signals the end of the previous frame
			
			int prevIdx = (foundIdx-1+bufSize)%bufSize;

			if (m_extendedSequenceNumbers[prevIdx] + 1 == seqNr)
			{
				if (m_packetParts[prevIdx] != 0 && m_timestamps[prevIdx] != timestamp) // got a stored packet part with a different timestamp
				{
					std::vector<uint8_t *> parts;
					std::vector<size_t> partSizes;
					std::vector<real_t> receiveTimes;
					uint32_t packetTimestamp = 0;

					checkCompletePacket(prevIdx, parts, partSizes, receiveTimes, packetTimestamp);

					if (parts.size() != 0)
					{
						m_partQueue.push_back(parts);
						m_partSizeQueue.push_back(partSizes);
						m_timestampQueue.push_back(packetTimestamp);
						m_receiveTimesQueue.push_back(receiveTimes);
					}
				}
			}

			// check current frame
			{
				std::vector<uint8_t *> parts;
				std::vector<size_t> partSizes;
				std::vector<real_t> receiveTimes;
				uint32_t packetTimestamp = 0;

				checkCompletePacket(foundIdx, parts, partSizes, receiveTimes, packetTimestamp);

				if (parts.size() != 0)
				{
					m_partQueue.push_back(parts);
					m_partSizeQueue.push_back(partSizes);
					m_timestampQueue.push_back(packetTimestamp);
					m_receiveTimesQueue.push_back(receiveTimes);
				}
			}

			// check if this packet signals the start of the next frame

			int nextIdx = (foundIdx + 1) % bufSize;

			if (seqNr + 1 == m_extendedSequenceNumbers[nextIdx])
			{
				if (m_packetParts[nextIdx] != 0 && m_timestamps[nextIdx] != timestamp)
				{
					std::vector<uint8_t *> parts;
					std::vector<size_t> partSizes;
					std::vector<real_t> receiveTimes;
					uint32_t packetTimestamp = 0;

					checkCompletePacket(nextIdx, parts, partSizes, receiveTimes, packetTimestamp);

					if (parts.size() != 0)
					{
						m_partQueue.push_back(parts);
						m_partSizeQueue.push_back(partSizes);
						m_timestampQueue.push_back(packetTimestamp);
						m_receiveTimesQueue.push_back(receiveTimes);
					}
				}
			}

			checkMaximumQueueLength();
		}
	}

	//printTable();

	uint32_t seqDif = seqNr - m_extendedSequenceNumbers[m_startPosition];

	if (seqDif < 0x80000000) // positive number
	{
		if (seqDif > bufSize*2/3)
		{
			uint32_t newStartSeqNr = seqNr - bufSize/3;
			int diff = newStartSeqNr - m_extendedSequenceNumbers[m_startPosition];

			if (diff >= bufSize) // clear everything
			{
				m_startPosition = -1;

				for (int i = 0 ; i < bufSize ; i++)
				{
					m_extendedSequenceNumbers[i] = 0;
					m_timestamps[i] = 0;
					m_skipFlags[i] = false;
					if (m_packetParts[i])
						delete [] m_packetParts[i];

					m_packetParts[i] = 0;
					m_partSizes[i] = 0;
					m_markers[i] = false;
					m_firstPartMarkers[i] = false;
					m_receiveTimes[i] = 0;
				}
			}
			else
			{
				int lastPos = (m_startPosition-1+bufSize)%bufSize;
				uint32_t newSeqNr = m_extendedSequenceNumbers[lastPos] + 1;

				for (int i = 0 ; i < diff ; i++, newSeqNr++)
				{
					m_extendedSequenceNumbers[m_startPosition] = newSeqNr;
					m_timestamps[m_startPosition] = 0;
					m_skipFlags[m_startPosition] = false;
					
					if (m_packetParts[m_startPosition])
						delete [] m_packetParts[m_startPosition];
					m_packetParts[m_startPosition] = 0;
					
					m_partSizes[m_startPosition] = 0;
					m_markers[m_startPosition] = false;
					m_firstPartMarkers[m_startPosition] = false;
					m_receiveTimes[m_startPosition] = 0;

					m_startPosition = (m_startPosition+1)%bufSize;
				}
			}
		}
	}

	return true;
}

void MIPRTPPacketGrouper::checkCompletePacket(int index, std::vector<uint8_t *> &parts, std::vector<size_t> &partSizes, std::vector<real_t> &receiveTimes, uint32_t &packetTimestamp)
{
	int startIdx = -1;
	int endIdx = -1;
	bool done = false;

	int idx = index;
	int counter = 0;

	int bufSize = m_extendedSequenceNumbers.size();
	uint32_t seqNr = m_extendedSequenceNumbers[index];
	uint32_t timestamp = m_timestamps[index];

	while (!done && counter < bufSize)
	{
		if (m_packetParts[idx] == 0)
		{
			done = true;

			if (m_skipFlags[idx]) // got the packet and deallocated it already
			{
				if (seqNr + counter == m_extendedSequenceNumbers[idx] && m_timestamps[idx] != timestamp)
					endIdx = (idx-1+bufSize)%bufSize;
			}
		}
		else
		{
			if (seqNr + counter != m_extendedSequenceNumbers[idx])
				done = true;
			else
			{
				if (m_timestamps[idx] != timestamp)
				{
					done = true;
					endIdx = (idx-1+bufSize)%bufSize;
				}
				else
				{
					if (m_markers[idx]) // assume that marker indicates end of frame
					{
						done = true;
						endIdx = idx;
					}
				}
			}
		}

		counter++;
		idx++;
		idx = idx%bufSize;
	}

	if (endIdx != -1)
	{
		done = false;
		counter = 0;
		idx = index;

		while (!done && counter < bufSize)
		{
			counter++;
			idx--;
			idx = (idx + bufSize)%bufSize; // make sure it stays in the right range

			if (m_packetParts[idx] == 0)
			{
				done = true;

				if (m_skipFlags[idx]) // got the packet and deallocated it already
				{
					if (m_extendedSequenceNumbers[idx] + counter == seqNr && m_timestamps[idx] != timestamp)
						startIdx = (idx+1)%bufSize;
				}
			}
			else
			{
				if (m_extendedSequenceNumbers[idx] + counter != seqNr)
					done = true;
				else
				{
					if (m_timestamps[idx] != timestamp)
					{
						done = true;
						startIdx = (idx+1)%bufSize;
					}
					else
					{
						if (m_firstPartMarkers[idx])
						{
							done = true;
							startIdx = idx;
						}
					}
				}
			}
		}
	}

	parts.resize(0);
	partSizes.resize(0);
	receiveTimes.resize(0);

	if (startIdx != -1 && endIdx != -1)
	{
		int idx = startIdx;
		bool done = false;

		packetTimestamp = m_timestamps[startIdx];

		while (!done)
		{
			parts.push_back(m_packetParts[idx]);
			partSizes.push_back(m_partSizes[idx]);
			receiveTimes.push_back(m_receiveTimes[idx]);

			m_packetParts[idx] = 0;
			m_skipFlags[idx] = true;

			if (idx == endIdx)
				done = true;
			else
				idx = (idx+1)%bufSize;
		}
	}
}

void MIPRTPPacketGrouper::checkMaximumQueueLength()
{
	// TODO
}

/*
void MIPRTPPacketGrouper::printTable()
{
	int bufSize = m_extendedSequenceNumbers.size();

	for (int i = 0 ; i < bufSize ; i++)
	{
		if (i == m_startPosition)
			std::cout << "* " ;
		else
			std::cout << "  " ;
		
		std::cout << m_extendedSequenceNumbers[i] << "\t";
		std::cout << m_timestamps[i] << "\t";
		std::cout << (int)m_firstPartMarkers[i] << " ";
		std::cout << (int)m_markers[i] << " ";
		if (m_packetParts[i])
			std::cout << (void *)m_packetParts[i] << "\t";
		else
			std::cout << "         \t";
		std::cout << m_partSizes[i] << "\t";
		std::cout << m_skipFlags[i] << std::endl;
	}
	std::cout << std::endl;
}
*/

void MIPRTPPacketGrouper::getNextQueuedPacket(std::vector<uint8_t *> &parts, std::vector<size_t> &partSizes, uint32_t &timestamp)
{
	if (m_partQueue.empty())
	{
		parts.resize(0);
		partSizes.resize(0);
		timestamp = 0;
	}
	else
	{
		parts = *(m_partQueue.begin());
		partSizes = *(m_partSizeQueue.begin());
		timestamp = *(m_timestampQueue.begin());

		m_partQueue.pop_front();
		m_partSizeQueue.pop_front();
		m_timestampQueue.pop_front();
		m_receiveTimesQueue.pop_front(); // don't forget to pop this!
	}
}

void MIPRTPPacketGrouper::getNextQueuedPacket(std::vector<uint8_t *> &parts, std::vector<size_t> &partSizes, std::vector<real_t> &receiveTimes, uint32_t &timestamp)
{
	if (m_partQueue.empty())
	{
		parts.resize(0);
		partSizes.resize(0);
		receiveTimes.resize(0);
		timestamp = 0;
	}
	else
	{
		parts = *(m_partQueue.begin());
		partSizes = *(m_partSizeQueue.begin());
		timestamp = *(m_timestampQueue.begin());
		receiveTimes = *(m_receiveTimesQueue.begin());

		m_partQueue.pop_front();
		m_partSizeQueue.pop_front();
		m_timestampQueue.pop_front();
		m_receiveTimesQueue.pop_front();
	}
}

