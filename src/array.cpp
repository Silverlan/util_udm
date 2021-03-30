/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>
#pragma optimize("",off)
udm::Array::~Array() {Clear();}

bool udm::Array::operator==(const Array &other) const
{
	auto res = (m_size == other.m_size && m_valueType == other.m_valueType);
	UDM_ASSERT_COMPARISON(res);
	if(res == false)
		return false;
	// Note: If this is a compressed array, calling GetValues may invoke decompression!
	auto *values = GetValues();
	auto *valuesOther = other.GetValues();
	if(is_trivial_type(m_valueType) || m_valueType == Type::Struct)
	{
		auto sz = GetByteSize();
		res = (memcmp(values,valuesOther,sz) == 0);
		UDM_ASSERT_COMPARISON(res);
		return res;
	}
	auto tag = get_non_trivial_tag(m_valueType);
	return std::visit([this,&other,values,valuesOther](auto tag) -> bool {
		using T = decltype(tag)::type;
		auto *ptr0 = static_cast<const T*>(values);
		auto *ptr1 = static_cast<const T*>(valuesOther);
		for(auto i=decltype(m_size){0u};i<m_size;++i)
		{
			auto res = (*ptr0 == *ptr1);
			UDM_ASSERT_COMPARISON(res);
			if(res == false)
				return false;
			++ptr0;
			++ptr1;
		}
		return true;
	},tag);
}

udm::Array &udm::Array::operator=(Array &&other)
{
	if(this == &other)
		return *this;
	Clear();
	m_valueType = other.m_valueType;
	m_size = other.m_size;
	m_values = other.m_values;
	fromProperty = other.fromProperty;

	other.m_values = nullptr;
	other.m_size = 0;
	other.m_valueType = Type::Nil;
	static_assert(sizeof(*this) == 40,"Update this function when the struct has changed!");
	return *this;
}
udm::Array &udm::Array::operator=(const Array &other)
{
	if(this == &other)
		return *this;
	Clear();
	SetValueType(other.m_valueType);
	Merge(other);
	fromProperty = other.fromProperty;
	static_assert(sizeof(*this) == 40,"Update this function when the struct has changed!");
	return *this;
}

void udm::Array::Merge(const Array &other)
{
	if(m_valueType != other.m_valueType)
		return;
	auto size = GetSize();
	auto sizeOther = other.GetSize();
	auto offset = size;
	Resize(size +sizeOther);

	if(is_trivial_type(m_valueType))
	{
		memcpy(GetValuePtr(offset),other.GetValues(),size_of(m_valueType) *sizeOther);
		return;
	}

	std::visit([this,&other,offset,sizeOther](auto tag) {
		using T = decltype(tag)::type;
		auto *ptrDst = static_cast<T*>(GetValuePtr(offset));
		auto *ptrSrc = static_cast<const T*>(other.GetValues());
		for(auto i=decltype(sizeOther){0u};i<sizeOther;++i)
			ptrDst[i] = ptrSrc[i];
	},get_non_trivial_tag(m_valueType));
}

void udm::Array::SetValueType(Type valueType)
{
	if(valueType == m_valueType)
		return;
	Clear();
	m_valueType = valueType;
	Resize(GetSize());
}

uint32_t udm::Array::GetValueSize() const
{
	return (m_valueType == Type::Struct) ? GetStructuredDataInfo()->GetDataSizeRequirement() : size_of_base_type(m_valueType);
}

void *udm::Array::GetValuePtr(uint32_t idx)
{
	return static_cast<uint8_t*>(GetValues()) +idx *GetValueSize();
}

void udm::Array::Resize(uint32_t newSize)
{
	auto isStructType = (m_valueType == Type::Struct);
	if(newSize == m_size && (!isStructType || m_values))
		return;
	auto headerSize = GetHeaderSize();
	if(is_non_trivial_type(m_valueType))
	{
		auto tag = get_non_trivial_tag(m_valueType);
		return std::visit([this,newSize,isStructType,headerSize](auto tag) {
			using T = decltype(tag)::type;
			auto *newValues = AllocateData(newSize *sizeof(T));
			//for(auto i=decltype(newSize){0u};i<newSize;++i)
			//	new (&newValues[i]) T{};
			auto *dataPtr = reinterpret_cast<T*>(newValues);
			if(isStructType)
			{
				auto *strct = GetStructuredDataInfo();
				if(strct)
					**reinterpret_cast<StructDescription**>(dataPtr) = std::move(*strct);
				dataPtr = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(dataPtr) +sizeof(StructDescription**));
			}

			auto *curValues = GetValues();
			if(curValues)
			{
				auto numCpy = umath::min(newSize,m_size);
				for(auto i=decltype(numCpy){0u};i<numCpy;++i)
					dataPtr[i] = std::move(const_cast<T*>(static_cast<T*>(curValues))[i]);
				for(auto i=m_size;i<newSize;++i)
					dataPtr[i] = T{}; // Default initialize
			}
			Clear();
			m_values = newValues;
			m_size = newSize;
			
			// Also see udm::Property::Read (with array overload)
			if constexpr(std::is_same_v<T,Element> || is_array_type(type_to_enum<T>()))
			{
				auto *p = dataPtr;
				for(auto i=decltype(m_size){0u};i<m_size;++i)
					p[i].fromProperty = PropertyWrapper{*this,i};
			}
		},tag);
		return;
	}

	auto sizeBytes = newSize *size_of(m_valueType);
	auto *newValues = AllocateData(sizeBytes);
	auto *curValues = GetValues();
	if(curValues)
	{
		auto *dataPtr = newValues;
		if(isStructType)
			dataPtr += sizeof(StructDescription**);
		auto cpySizeBytes = umath::min(m_size *size_of(m_valueType),sizeBytes);
		memcpy(dataPtr,curValues,cpySizeBytes);
		if(sizeBytes > cpySizeBytes)
			memset(dataPtr +cpySizeBytes,0,sizeBytes -cpySizeBytes);
		Clear();
	}
	m_values = newValues;
	m_size = newSize;
}

udm::PropertyWrapper udm::Array::operator[](uint32_t idx)
{
	return PropertyWrapper{*this,idx};
}

void *udm::Array::GetValuePtr()
{
	return m_values && m_size > 0 ? (static_cast<uint8_t*>(m_values) +GetHeaderSize()) : nullptr;
}
void *udm::Array::GetHeaderPtr()
{
	return (GetHeaderSize() > 0 && m_values) ? m_values : nullptr;
}
uint64_t udm::Array::GetHeaderSize() const
{
	switch(GetValueType())
	{
	case Type::Struct:
		return sizeof(StructDescription**);
	}
	return 0;
}
uint64_t udm::Array::GetByteSize() const
{
	if(GetValueType() == Type::Struct)
	{
		auto *strct = GetStructuredDataInfo();
		if(strct)
			return GetSize() *static_cast<uint32_t>(strct->GetDataSizeRequirement());
		throw ImplementationError{"Structured LZ4 array has invalid structure data info!"}; // Unreachable
	}
	return GetSize() *size_of(m_valueType);
}
udm::StructDescription *udm::Array::GetStructuredDataInfo()
{
	auto *p = (m_valueType == Type::Struct) ? static_cast<StructDescription**>(GetHeaderPtr()) : nullptr;
	return p ? *p : nullptr;
}
void udm::Array::Clear()
{
	if(m_values == nullptr)
		return;
	ReleaseValues();
	m_size = 0;
}
uint8_t *udm::Array::AllocateData(uint64_t size) const
{
	auto *ptr = new uint8_t[size +GetHeaderSize()];
	if(m_valueType == Type::Struct)
		new (ptr) StructDescription*{new StructDescription{}};
	if(!is_trivial_type(m_valueType) && m_valueType != Type::Struct) // Structs are a special case where the array data only consists of trivial types; No default constructor required
	{
		// We'll have to default-initialize non-trivial data ourselves
		auto *tmp = ptr +GetHeaderSize();
		std::visit([tmp,size](auto tag) {
			using T = decltype(tag)::type;
			auto *p = reinterpret_cast<T*>(tmp);
			if((size %sizeof(T)) != 0)
				throw ImplementationError{"Array allocation size does not match multiple of size of value type!"};
			auto n = size /sizeof(T);
			for(auto i=decltype(n){0u};i<n;++i)
				new (&p[i]) T{};
		},get_non_trivial_tag(m_valueType));
	}
	return ptr;
}
void udm::Array::ReleaseValues()
{
	if(m_values == nullptr)
		return;
	if(m_valueType == Type::Struct)
		delete *static_cast<StructDescription**>(m_values);

	if(is_non_trivial_type(m_valueType) && m_valueType != Type::Struct) // Structs are a special case where the array data only consists of trivial types; No destructor required
	{
		// Call destructor for non-trivial data
		auto *tmp = static_cast<uint8_t*>(m_values) +GetHeaderSize();
		std::visit([this,tmp](auto tag) {
			using T = decltype(tag)::type;
			auto *p = reinterpret_cast<T*>(tmp);
			for(auto i=decltype(m_size){0u};i<m_size;++i)
				p[i].~T();
		},get_non_trivial_tag(m_valueType));
	}

	delete[] static_cast<uint8_t*>(m_values);
	m_values = nullptr;
}

//////////////////

void udm::ArrayLz4::InitializeSize(uint32_t size) {m_size = size;}
udm::StructDescription *udm::ArrayLz4::GetStructuredDataInfo()
{
	auto *strct = Array::GetStructuredDataInfo();
	if(strct)
		return strct;
	return m_structuredDataInfo.get();
}
void udm::ArrayLz4::ClearUncompressedMemory() {Compress();}
void udm::ArrayLz4::Compress()
{
	if(m_state == State::Compressed)
		return;
	m_state = State::Compressed;
	auto *p = GetValuePtr();
	if(!p)
		return;
	m_compressedBlob = udm::compress_lz4_blob(p,GetByteSize());
	auto *pStrct = Array::GetStructuredDataInfo();
	if(pStrct)
	{
		m_structuredDataInfo = std::make_unique<StructDescription>();
		*m_structuredDataInfo = std::move(*pStrct);
	}
	ReleaseValues();
}
void udm::ArrayLz4::Decompress()
{
	if(m_state == State::Uncompressed)
		return;
	if(GetValuePtr())
		throw ImplementationError{"Attempted to decompress array which has a valid data pointer!"}; // Unreachable
	m_state = State::Uncompressed;
	auto uncompressedSize = GetByteSize();
	if(uncompressedSize > 0)
	{
		m_values = AllocateData(uncompressedSize);
		udm::decompress_lz4_blob(m_compressedBlob.compressedData.data(),m_compressedBlob.compressedData.size(),uncompressedSize,GetValuePtr());
		if(m_structuredDataInfo)
		{
			auto *pStrct = Array::GetStructuredDataInfo();
			assert(pStrct);
			if(!pStrct)
				throw ImplementationError{"Structured data mismatch between LZ4 array and base array!"};
			*pStrct = std::move(*m_structuredDataInfo);
			m_structuredDataInfo = nullptr;
		}
	}
	m_compressedBlob = {};
}
udm::ArrayLz4 &udm::ArrayLz4::operator=(Array &&other)
{
	auto *otherLz4 = dynamic_cast<const ArrayLz4*>(&other);
	if(otherLz4)
		return operator=(std::move(*otherLz4));
	Array::operator=(std::move(other));
	return *this;
}
udm::ArrayLz4 &udm::ArrayLz4::operator=(const Array &other)
{
	auto *otherLz4 = dynamic_cast<const ArrayLz4*>(&other);
	if(otherLz4)
		return operator=(*otherLz4);
	Array::operator=(other);
	return *this;
}
udm::ArrayLz4 &udm::ArrayLz4::operator=(ArrayLz4 &&other)
{
	m_compressedBlob = std::move(other.m_compressedBlob);
	Array::operator=(std::move(other));
	return *this;
}
udm::ArrayLz4 &udm::ArrayLz4::operator=(const ArrayLz4 &other)
{
	m_compressedBlob = other.m_compressedBlob;
	Array::operator=(other);
	return *this;
}
void udm::ArrayLz4::SetValueType(Type valueType)
{
	if(!is_trivial_type(valueType) && valueType != Type::Struct)
		throw InvalidUsageError{"Attempted to create compressed array of type '" +std::string{magic_enum::enum_name(valueType)} +"', which is not a trivial type! Only trivial types are allowed for compressed arrays!"};
	Array::SetValueType(valueType);
}
udm::BlobLz4 &udm::ArrayLz4::GetCompressedBlob()
{
	Compress();
	return m_compressedBlob;
}
void *udm::ArrayLz4::GetValues()
{
	Decompress();
	return Array::GetValues();
}
void udm::ArrayLz4::Clear()
{
	m_structuredDataInfo = nullptr;
	if(m_state == State::Compressed)
	{
		m_compressedBlob = {};
		return;
	}
	Array::Clear();
}
#pragma optimize("",on)
