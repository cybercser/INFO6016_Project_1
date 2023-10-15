#include "buffer.h"

namespace network
{
	Buffer::Buffer(uint32 size)
		: m_WriteIndex(0), m_ReadIndex(0)
	{
		m_Data.resize(size, 0);
	}

	Buffer::~Buffer()
	{
		m_Data.clear();
	}

	void Buffer::WriteUInt32LE(size_t index, uint32 value)
	{
		// grow when serializing past the write index
		size_t oldSize = m_Data.size();
		if (index + sizeof(value) > oldSize) {
			m_Data.resize(oldSize + kGROW_SIZE);
		}

		m_Data[index] = value;
		m_Data[index + 1] = value >> 8;
		m_Data[index + 2] = value >> 16;
		m_Data[index + 3] = value >> 24;
	}

	void Buffer::WriteUInt32LE(uint32 value)
	{
		WriteUInt32LE(m_WriteIndex, value);
		m_WriteIndex += 4;
	}

	void Buffer::WriteUInt16LE(size_t index, uint16 value)
	{
		// grow when serializing past the write index
		size_t oldSize = m_Data.size();
		if (index + sizeof(value) > oldSize) {
			m_Data.resize(oldSize + kGROW_SIZE);
		}

		m_Data[index] = value;
		m_Data[index + 1] = value >> 8;
	}

	void Buffer::WriteUInt16LE(uint16 value)
	{
		WriteUInt16LE(m_WriteIndex, value);
		m_WriteIndex += 2;
	}

	void Buffer::WriteString(size_t index, const std::string& str, uint32 strLen)
	{
		// grow when serializing past the write index
		size_t oldSize = m_Data.size();
		if (index + strLen > oldSize) {
			size_t newSize = oldSize + kGROW_SIZE;
			while (newSize < index + strLen) {
				newSize += kGROW_SIZE;
			}
			m_Data.resize(newSize);
		}

		str.copy((char*)&m_Data[index], strLen);
	}

	void Buffer::WriteString(const std::string& str, uint32 strLen)
	{
		WriteString(m_WriteIndex, str, strLen);
		m_WriteIndex += str.size();
	}

	uint32 Buffer::ReadUInt32LE(size_t index)
	{
		uint32 newValue = 0;
		newValue |= m_Data[index];
		newValue |= m_Data[index + 1] << 8;
		newValue |= m_Data[index + 2] << 16;
		newValue |= m_Data[index + 3] << 24;

		return newValue;
	}

	uint32 Buffer::ReadUInt32LE()
	{
		uint32 newValue = ReadUInt32LE(m_ReadIndex);
		m_ReadIndex += 4;

		return newValue;
	}

	uint16 Buffer::ReadUInt16LE(size_t index)
	{
		uint16 newValue = 0;
		newValue |= m_Data[index];
		newValue |= m_Data[index + 1] << 8;

		return newValue;
	}

	uint16 Buffer::ReadUInt16LE()
	{
		uint16 newValue = ReadUInt16LE(m_ReadIndex);
		m_ReadIndex += 2;

		return newValue;
	}

	std::string Buffer::ReadString(size_t index, uint32 strLen)
	{
		std::string newStr{ "" };
		newStr.assign((const char*)&m_Data[index], strLen);
		return newStr;
	}

	std::string Buffer::ReadString(uint32 strLen)
	{
		std::string newStr = ReadString(m_ReadIndex, strLen);
		m_ReadIndex += newStr.size();

		return newStr;
	}

	char* Buffer::Data()
	{
		return (char*)m_Data.data();
	}

	const char* Buffer::ConstData()
	{
		return (const char*)m_Data.data();
	}

	size_t Buffer::Size() const
	{
		return m_Data.size();
	}

	void Buffer::Reset()
	{
		m_Data.clear();
	}
}