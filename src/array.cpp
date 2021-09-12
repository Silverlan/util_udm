/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>
#include <sharedutils/datastream.h>
#include <sharedutils/scope_guard.h>

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

void udm::Array::Merge(const Array &other,MergeFlags mergeFlags)
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

void udm::Array::SetValue(uint32_t idx,const void *value)
{
	auto vs = [&](auto tag){SetValue(idx,*static_cast<const decltype(tag)::type*>(value));};
	visit(m_valueType,vs);
}

void udm::Array::InsertValue(uint32_t idx,void *value)
{
	auto vs = [&](auto tag){InsertValue(idx,*static_cast<const decltype(tag)::type*>(value));};
	visit(m_valueType,vs);
}

void udm::Array::RemoveValue(uint32_t idx)
{
	auto size = GetSize();
	if(idx >= size)
		return;
	Range r0 {0 /* src */,0 /* dst */,idx};
	Range r1 {idx +1 /* src */,idx /* dst */,size -1 -idx};
	Resize(size -1,r0,r1,false);
}

template<typename T>
	static void copy_non_trivial_data(const void *curValues,void *dataPtr,uint32_t sizeOfElement,uint32_t idxStartSrc,uint32_t idxStartDst,uint32_t count) {
	for(uint32_t i=0;i<count;++i)
		static_cast<T*>(dataPtr)[idxStartDst +i] = std::move(const_cast<T*>(static_cast<const T*>(curValues))[idxStartSrc +i]);
};

void udm::Array::Resize(uint32_t newSize,Range r0,Range r1,bool defaultInitializeNewValues)
{
	auto isStructType = (m_valueType == Type::Struct);
	if(newSize == m_size && (!isStructType || m_values))
		return;
	auto headerSize = GetHeaderSize();
	auto cpyData = [&r0,&r1](const void *curValues,void *dataPtr,uint32_t sizeOfElement,void(*fCpy)(const void*,void*,uint32_t,uint32_t,uint32_t,uint32_t)) {
		if(std::get<0>(r1) == std::get<0>(r0) +std::get<2>(r0) && std::get<1>(r1) == std::get<1>(r0) +std::get<2>(r0)) // Check if no gap between both ranges (i.e. startSrc1 = startSrc0 +count0)
			fCpy(curValues,dataPtr,sizeOfElement,std::get<0>(r0),std::get<1>(r0),std::get<2>(r0) +std::get<2>(r1)); // We can copy both ranges at once
		else
		{
			fCpy(curValues,dataPtr,sizeOfElement,std::get<0>(r0),std::get<1>(r0),std::get<2>(r0));
			fCpy(curValues,dataPtr,sizeOfElement,std::get<0>(r1),std::get<1>(r1),std::get<2>(r1));
		}
	};
	if(is_non_trivial_type(m_valueType))
	{
		auto tag = get_non_trivial_tag(m_valueType);
		return std::visit([this,newSize,isStructType,headerSize,defaultInitializeNewValues,&r0,&r1,&cpyData](auto tag) mutable {
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
				cpyData(curValues,dataPtr,0,copy_non_trivial_data<T>);
				if constexpr(is_non_trivial_type(type_to_enum<T>()))
					defaultInitializeNewValues = false; // Non-trivial data has already been default-initialized by AllocateData
				if(defaultInitializeNewValues)
				{
					for(uint32_t i=0;i<std::get<1>(r0);++i)
						dataPtr[i] = T{}; // Default initialize all new prefix items
					for(auto i=std::get<1>(r0) +std::get<2>(r0);i<std::get<1>(r1);++i)
						dataPtr[i] = T{}; // Default initialize all items within ranges
					for(auto i=std::get<1>(r1) +std::get<2>(r1);i<newSize;++i)
						dataPtr[i] = T{}; // Default initialize all new postfix items
				}
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
		auto sizeOfElement = size_of(m_valueType);
		cpyData(curValues,dataPtr,sizeOfElement,[](const void *curValues,void *dataPtr,uint32_t sizeOfElement,uint32_t idxStartSrc,uint32_t idxStartDst,uint32_t count) {
			memcpy(static_cast<uint8_t*>(dataPtr) +idxStartDst *sizeOfElement,static_cast<const uint8_t*>(curValues) +idxStartSrc *sizeOfElement,count *sizeOfElement);
		});
		if(defaultInitializeNewValues)
		{
			memset(dataPtr,0,std::get<1>(r0) *sizeOfElement); // Default initialize all new prefix items
			memset(dataPtr +(std::get<1>(r0) +std::get<2>(r0)) *sizeOfElement,0,(std::get<1>(r1) -(std::get<1>(r0) +std::get<2>(r0))) *sizeOfElement); // Default initialize all items within ranges
			memset(dataPtr +(std::get<1>(r1) +std::get<2>(r1)) *sizeOfElement,0,(newSize -(std::get<1>(r1) +std::get<2>(r1))) *sizeOfElement); // Default initialize all new postfix items
		}
		Clear();
	}
	m_values = newValues;
	m_size = newSize;
}

void udm::Array::Resize(uint32_t newSize)
{
	auto size = umath::min(GetSize(),newSize);
	Range r0 {0,0,size};
	Range r1 {size,size,0};
	Resize(newSize,r0,r1,true);
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

struct StreamData
	: public udm::IFile
{
	StreamData()=default;
	DataStream &GetDataStream() {return m_ds;}
	virtual size_t Read(void *data,size_t size) override
	{
		m_ds->Read(data,size);
		return size;
	}
	virtual size_t Write(const void *data,size_t size) override
	{
		m_ds->Write(static_cast<const uint8_t*>(data),size);
		return size;
	}
	virtual size_t Tell() override
	{
		return m_ds->GetOffset();
	}
	virtual void Seek(size_t offset,Whence whence) override
	{
		switch(whence)
		{
		case Whence::Set:
			m_ds->SetOffset(offset);
			break;
		case Whence::End:
			m_ds->SetOffset(m_ds->GetInternalSize() +offset);
			break;
		case Whence::Cur:
			m_ds->SetOffset(m_ds->GetOffset() +offset);
			break;
		}
	}
	virtual int32_t ReadChar() override
	{
		return m_ds->Read<char>();
	}
private:
	DataStream m_ds;
};

#pragma pack(push,1)
struct CompressedStringArrayHeader
{
	uint32_t numStrings;
};
#pragma pack(pop)
void udm::ArrayLz4::Compress()
{
	if(m_state == State::Compressed)
		return;
	util::ScopeGuard sgState {[this]() {m_state = State::Compressed;}};
	auto *p = GetValuePtr();
	if(!p)
		return;
	if(m_valueType == Type::Element)
	{
		auto n = GetSize();
		StreamData f {};
		f.IFile::Write<uint32_t>(n);
		for(auto it=begin<Element>();it!=end<Element>();++it)
		{
			auto &el = *it;
			Property::Write(f,el);
		}
		auto &ds = f.GetDataStream();
		m_compressedBlob = udm::compress_lz4_blob(ds->GetData(),ds->GetInternalSize());
		ReleaseValues();
		return;
	}

	if(m_valueType == Type::String)
	{
		auto *pString = static_cast<String*>(GetValuePtr(0));
		auto n = GetSize();
		size_t sizeInBytes = 0;
		auto *p = pString;
		for(auto i=decltype(n){0u};i<n;++i)
		{
			sizeInBytes += Property::GetStringSizeRequirement(*p);
			++p;
		}
		sizeInBytes += sizeof(CompressedStringArrayHeader);

		VectorFile memFile {sizeInBytes};
		memFile.GetValueAndAdvance<CompressedStringArrayHeader>().numStrings = GetSize();

		p = pString;
		for(auto i=decltype(n){0u};i<n;++i)
		{
			Property::Write(memFile,*p);
			++p;
		}
		m_compressedBlob = udm::compress_lz4_blob(memFile.GetData(),memFile.GetDataSize());
		ReleaseValues();
		return;
	}

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

	if(m_valueType == Type::Element)
	{
		StreamData f {};
		auto &ds = f.GetDataStream();
		ds->Resize(m_compressedBlob.uncompressedSize);
		udm::decompress_lz4_blob(m_compressedBlob.compressedData.data(),m_compressedBlob.compressedData.size(),m_compressedBlob.uncompressedSize,ds->GetData());
		ds->SetOffset(0);

		auto numElements = f.IFile::Read<uint32_t>();
		m_values = AllocateData(numElements *sizeof(Element));
		auto prop = fromProperty;
		for(auto i=decltype(numElements){0u};i<numElements;++i)
			prop->Read(f,GetValue<Element>(i));
		m_compressedBlob = {};
		return;
	}

	if(m_valueType == Type::String)
	{
		VectorFile f {m_compressedBlob.uncompressedSize};
		udm::decompress_lz4_blob(m_compressedBlob.compressedData.data(),m_compressedBlob.compressedData.size(),m_compressedBlob.uncompressedSize,f.GetData());
		auto &header = f.GetValueAndAdvance<CompressedStringArrayHeader>();
		m_values = AllocateData(header.numStrings *sizeof(String));
		m_compressedBlob = {};
		
		auto *pString = static_cast<String*>(GetValuePtr(0));
		for(auto i=decltype(header.numStrings){0u};i<header.numStrings;++i)
		{
			Property::Read(f,*pString);
			++pString;
		}
		return;
	}

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
	if(!is_trivial_type(valueType) && valueType != Type::Struct && valueType != Type::Element && valueType != Type::String)
		throw InvalidUsageError{"Attempted to create compressed array of type '" +std::string{magic_enum::enum_name(valueType)} +"', which is not a supported non-trivial type!"};
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
