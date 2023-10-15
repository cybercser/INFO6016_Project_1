#pragma once

#include "common.h"

#include <vector>
#include <string>

namespace network
{
	// A buffer capable of serializing/deserializing uint16, uint32 and string, and 
	// grow when serrializing overflows.
	class Buffer
	{
	public:
		Buffer(uint32 size = 512);
		~Buffer();

		void WriteUInt32LE(uint32 value);
		void WriteUInt16LE(uint16 value);
		void WriteString(const std::string& str, uint32 strLen);
		uint32 ReadUInt32LE();
		uint16 ReadUInt16LE();
		std::string ReadString(uint32 strLen);
		char* Data();
		const char* ConstData();
		size_t Size() const;
		void Reset();

	private:
		void WriteUInt32LE(size_t index, uint32 value);
		void WriteUInt16LE(size_t index, uint16 value);
		void WriteString(size_t index, const std::string& str, uint32 strLen);
		uint32 ReadUInt32LE(size_t index);
		uint16 ReadUInt16LE(size_t index);
		std::string ReadString(size_t index, uint32 strLen);

	private:
		// this stores all of the data within the buffer
		std::vector<uint8> m_Data;

		// The index to write the next byte of data in the buffer
		uint32 m_WriteIndex;

		// The index to read the next byte of data from the buffer
		uint32 m_ReadIndex;

		// the size (in bytes) to grow when serializing past the write index
		static constexpr uint32 kGROW_SIZE = 256;
	};
}
