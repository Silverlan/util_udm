/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"

udm::MemoryFile::MemoryFile(uint8_t *data,size_t dataSize)
	: m_data{data},m_dataSize{dataSize}
{}
size_t udm::MemoryFile::Read(void *data,size_t size)
{
	if(m_pos >= m_dataSize)
		return 0;
	size = umath::min(m_dataSize -m_pos,size);
	memcpy(data,m_data +m_pos,size);
	m_pos += size;
	return size;
}
size_t udm::MemoryFile::Write(const void *data,size_t size)
{
	if(m_pos >= m_dataSize)
		return 0;
	size = umath::min(m_dataSize -m_pos,size);
	memcpy(m_data +m_pos,data,size);
	m_pos += size;
	return size;
}
size_t udm::MemoryFile::Tell() {return m_pos;}
void udm::MemoryFile::Seek(size_t offset,Whence whence)
{
	switch(whence)
	{
	case Whence::Set:
		m_pos = offset;
		break;
	case Whence::End:
		m_pos = m_dataSize +offset;
		break;
	case Whence::Cur:
		m_pos += offset;
		break;
	}
}
int32_t udm::MemoryFile::ReadChar()
{
	if(m_pos >= m_dataSize)
		return EOF;
	char c;
	Read(&c,sizeof(c));
	return c;
}
char *udm::MemoryFile::GetMemoryDataPtr() {return reinterpret_cast<char*>(m_data +m_pos);}

////////

udm::VectorFile::VectorFile()
	: MemoryFile{m_data.data(),m_data.size()}
{}
udm::VectorFile::VectorFile(size_t size)
	: VectorFile{}
{Resize(size);}
const std::vector<uint8_t> &udm::VectorFile::GetVector() const {return m_data;}
void udm::VectorFile::Resize(size_t size)
{
	m_data.resize(size);
	MemoryFile::m_data = m_data.data();
	m_dataSize = size;
}
