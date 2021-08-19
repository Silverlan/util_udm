/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sharedutils/base64.hpp>
#include <sstream>


udm::PProperty udm::Property::Create(Type type)
{
	auto prop = std::shared_ptr<Property>{new Property{}};
	prop->type = type;
	prop->Initialize();
	if(type == Type::Element)
		static_cast<Element*>(prop->value)->fromProperty = *prop;
	else if(is_array_type(type))
		static_cast<Array*>(prop->value)->fromProperty = *prop;
	return prop;
}

udm::Property::Property(const Property &other) {*this = other;}

udm::Property::Property(Property &&other)
{
	type = other.type;
	value = other.value;

	other.type = Type::Nil;
	other.value = nullptr;
}

udm::Property::~Property() {Clear();}

udm::Property &udm::Property::operator=(const Property &other)
{
	Clear();
	type = other.type;
	Initialize();
	if(is_trivial_type(type))
	{
		memcpy(value,other.value,size_of_base_type(type));
		return *this;
	}
	auto tag = get_non_trivial_tag(type);
	std::visit([this,&other](auto tag) {
		using T = decltype(tag)::type;
		*static_cast<T*>(value) = *static_cast<T*>(other.value);
	},tag);
	return *this;
}

void udm::Property::SetAppropriatePrecision(std::stringstream &ss,Type type)
{
	switch(type)
	{
	case Type::Float:
	case Type::Vector2:
	case Type::Vector3:
	case Type::Vector4:
	case Type::Quaternion:
	case Type::EulerAngles:
	case Type::Transform:
	case Type::ScaledTransform:
	case Type::Mat4:
	case Type::Mat3x4:
	case Type::Half:
		ss<<std::fixed;
		ss.precision(4);
		break;
	case Type::Double:
		ss<<std::fixed;
		ss.precision(8);
		break;
	}
	static_assert(umath::to_integral(Type::Count) == 36,"Update this list when new types are added!");
}
void udm::Property::RemoveTrailingZeroes(std::string &str)
{
	auto pos = str.find_last_not_of('0');
	if(pos == std::string::npos || pos == str.length() -1)
		return;
	auto sep = str.find('.');
	if(sep == std::string::npos)
		return;
	int offset{1};
	if(pos == sep)
		offset = 0;
	str.erase(pos +offset,std::string::npos);
}

void udm::Property::Initialize()
{
	Clear();
	if(is_non_trivial_type(type))
	{
		auto tag = get_non_trivial_tag(type);
		std::visit([&](auto tag){value = new decltype(tag)::type{};},tag);
		return;
	}
	if(type == Type::Nil)
		return;
	auto size = size_of(type);
	value = new uint8_t[size];
}

void udm::Property::Clear()
{
	if(value == nullptr)
		return;
	if(is_trivial_type(type))
		delete[] static_cast<uint8_t*>(value);
	else
	{
		if(is_non_trivial_type(type))
		{
			auto tag = get_non_trivial_tag(type);
			std::visit([&](auto tag){delete static_cast<decltype(tag)::type*>(this->value);},tag);
		}
	}
	value = nullptr;
}

bool udm::Property::Read(IFile &f,Blob &outBlob)
{
	auto size = f.Read<size_t>();
	outBlob.data.resize(size);
	f.Read(outBlob.data.data(),size);
	return true;
}
bool udm::Property::Read(IFile &f,BlobLz4 &outBlob)
{
	auto compressedSize = f.Read<size_t>();
	auto uncompressedSize = f.Read<size_t>();
	outBlob.uncompressedSize = uncompressedSize;
	outBlob.compressedData.resize(compressedSize);
	f.Read(outBlob.compressedData.data(),compressedSize);
	return true;
}
bool udm::Property::Read(IFile &f,Utf8String &outStr)
{
	auto size = f.Read<uint32_t>();
	outStr.data.resize(size);
	f.Read(outStr.data.data(),size);
	return true;
}
bool udm::Property::Read(IFile &f,Element &el)
{
	f.Seek(f.Tell() +sizeof(uint64_t)); // Skip size

	auto numChildren = f.Read<uint32_t>();
	std::vector<std::string> stringTable {};
	stringTable.resize(numChildren);
	for(auto i=decltype(numChildren){0u};i<numChildren;++i)
		stringTable[i] = Data::ReadKey(f);
	el.children.reserve(numChildren);
	for(auto i=decltype(numChildren){0u};i<numChildren;++i)
	{
		auto &name = stringTable[i];
		auto prop = Property::Create();
		if(prop->Read(f) == false)
			return false;
		el.children[std::move(name)] = prop;
	}
	return true;
}
bool udm::Property::Read(IFile &f,String &outStr)
{
	uint32_t len = f.Read<uint8_t>();
	if(len == EXTENDED_STRING_IDENTIFIER)
		len = f.Read<uint32_t>();
	outStr.resize(len);
	f.Read(outStr.data(),len);
	return true;
}
bool udm::Property::Read(IFile &f,Reference &outRef)
{
	String str;
	if(Read(f,str) == false)
		return false;
	outRef.path = std::move(str);
	return true;
}
bool udm::Property::ReadStructHeader(IFile &f,StructDescription &strct)
{
	auto n = f.Read<StructDescription::MemberCountType>();
	strct.types.resize(n);
	f.Read(strct.types.data(),n *sizeof(Type));
	strct.names.resize(n);
	for(auto i=decltype(n){0u};i<n;++i)
		Read(f,strct.names[i]);
	return true;
}
bool udm::Property::Read(IFile &f,Struct &strct)
{
	f.Seek(f.Tell() +sizeof(StructDescription::SizeType));
	ReadStructHeader(f,strct.description);
	auto dataSize = strct->GetDataSizeRequirement();
	strct.data.resize(dataSize);
	f.Read(strct.data.data(),dataSize);
	return true;
}
bool udm::Property::Read(IFile &f,Array &a)
{
	a.Clear();
	a.SetValueType(f.Read<decltype(a.GetValueType())>());
	a.Resize(f.Read<decltype(a.GetSize())>());
	auto *ptr = a.GetValues();
	a.fromProperty = {*this};
	if(is_non_trivial_type(a.GetValueType()))
	{
		f.Seek(f.Tell() +sizeof(uint64_t)); // Skip size

		if(a.GetValueType() == Type::Struct)
		{
			f.Seek(f.Tell() +sizeof(StructDescription::SizeType));
			auto *structInfo = a.GetStructuredDataInfo();
			assert(structInfo);
			if(!structInfo)
				throw ImplementationError{"Invalid array structure info!"};
			ReadStructHeader(f,*structInfo);

			auto szBytes = a.GetByteSize();
			f.Read(a.GetValues(),szBytes);
			return true;
		}
		else
		{
			auto tag = get_non_trivial_tag(a.GetValueType());
			return std::visit([this,&f,&a,ptr](auto tag) {
				using T = decltype(tag)::type;
				auto size = a.GetSize();
				for(auto i=decltype(size){0u};i<size;++i)
				{
					if(Read(f,static_cast<T*>(ptr)[i]) == false)
						return false;
				}

				// Also see udm::Array::Resize
				if constexpr(std::is_same_v<T,Element> || std::is_same_v<T,Array>)
				{
					auto *p = static_cast<T*>(ptr);
					for(auto i=decltype(size){0u};i<size;++i)
						p[i].fromProperty = PropertyWrapper{a,i};
				}
				return true;
			},tag);
		}
	}
	
	auto size = a.GetSize() *size_of(a.GetValueType());
	f.Read(ptr,size);
	return true;
}

bool udm::Property::Read(IFile &f,ArrayLz4 &a)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	auto compressedSize = f.Read<size_t>();
	a.Clear();
	a.m_valueType = f.Read<decltype(a.GetValueType())>();
	std::optional<size_t> uncompressedSize {};
	auto valueType = a.GetValueType();
	if(valueType == Type::Struct)
	{
		f.Seek(f.Tell() +sizeof(StructDescription::SizeType));
		a.m_structuredDataInfo = std::make_unique<StructDescription>();
		ReadStructHeader(f,*a.m_structuredDataInfo);
	}
	else if(valueType == Type::Element || valueType == Type::String)
		uncompressedSize = f.Read<size_t>();

	a.m_size = f.Read<decltype(a.GetSize())>();
	a.fromProperty = {*this};

	auto &blob = a.GetCompressedBlob();
	blob.uncompressedSize = uncompressedSize.has_value() ? *uncompressedSize : a.GetByteSize();
	blob.compressedData.resize(compressedSize);
	f.Read(blob.compressedData.data(),compressedSize);
	return true;
}

void udm::Property::Write(IFile &f,const Blob &blob)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f.Write<size_t>(blob.data.size());
	f.Write(blob.data.data(),blob.data.size());
}
void udm::Property::Write(IFile &f,const BlobLz4 &blob)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f.Write<size_t>(blob.compressedData.size());
	f.Write<size_t>(blob.uncompressedSize);
	f.Write(blob.compressedData.data(),blob.compressedData.size());
}
void udm::Property::Write(IFile &f,const Utf8String &str)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f.Write<uint32_t>(str.data.size());
	f.Write(str.data.data(),str.data.size());
}
void udm::Property::Write(IFile &f,const Element &el)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	auto offsetToSize = WriteBlockSize<uint64_t>(f);

	f.Write<uint32_t>(el.children.size());
	for(auto &pair : el.children)
		Data::WriteKey(f,pair.first);

	for(auto &pair : el.children)
		pair.second->Write(f);

	WriteBlockSize<uint64_t>(f,offsetToSize);
}
void udm::Property::Write(IFile &f,const Array &a)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f.Write(a.GetValueType());
	f.Write(a.GetSize());
	if(is_non_trivial_type(a.GetValueType()))
	{
		auto offsetToSize = WriteBlockSize<uint64_t>(f);

		if(a.GetValueType() == Type::Struct)
		{
			auto *structInfo = a.GetStructuredDataInfo();
			assert(structInfo);
			if(!structInfo)
				throw ImplementationError{"Invalid array structure info!"};
			auto offsetToSize = WriteBlockSize<StructDescription::SizeType>(f);
			WriteStructHeader(f,*structInfo);
			WriteBlockSize<StructDescription::SizeType>(f,offsetToSize);

			auto szBytes = a.GetByteSize();
			f.Write(a.GetValues(),szBytes);
		}
		else
		{
			auto tag = get_non_trivial_tag(a.GetValueType());
			std::visit([&](auto tag){
				using T = decltype(tag)::type;
				for(auto i=decltype(a.GetSize()){0u};i<a.GetSize();++i)
					Property::Write(f,static_cast<const T*>(a.GetValues())[i]);
			},tag);
		}

		WriteBlockSize<uint64_t>(f,offsetToSize);
		return;
	}
	f.Write(a.GetValues(),a.GetSize() *size_of(a.GetValueType()));
}
void udm::Property::Write(IFile &f,const ArrayLz4 &a)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	auto &blob = a.GetCompressedBlob();
	f.Write<size_t>(blob.compressedData.size());
	f.Write(a.GetValueType());

	auto valueType = a.GetValueType();
	if(valueType == Type::Struct)
	{
		auto *structInfo = a.GetStructuredDataInfo();
		assert(structInfo);
		if(!structInfo)
			throw ImplementationError{"Invalid array structure info!"};
		auto offsetToSize = WriteBlockSize<StructDescription::SizeType>(f);
		WriteStructHeader(f,*structInfo);
		WriteBlockSize<StructDescription::SizeType>(f,offsetToSize);
	}
	else if(valueType == Type::Element || valueType == Type::String)
		f.Write<size_t>(blob.uncompressedSize);

	f.Write(a.GetSize());
	f.Write(blob.compressedData.data(),blob.compressedData.size());
}
uint32_t udm::Property::GetStringPrefixSizeRequirement(const String &str)
{
	auto len = umath::min(str.length(),static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
	if(len < EXTENDED_STRING_IDENTIFIER)
		return sizeof(uint8_t);
	return sizeof(uint32_t);
}
uint32_t udm::Property::GetStringSizeRequirement(const String &str)
{
	return GetStringPrefixSizeRequirement(str) +str.length();
}
void udm::Property::Write(IFile &f,const String &str)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	auto len = umath::min(str.length(),static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
	if(len < EXTENDED_STRING_IDENTIFIER)
		f.Write<uint8_t>(len);
	else
	{
		f.Write<uint8_t>(EXTENDED_STRING_IDENTIFIER);
		f.Write<uint32_t>(len);
	}
	f.Write(str.data(),len);
}
void udm::Property::Write(IFile &f,const Reference &ref)
{
	Write(f,ref.path);
}
void udm::Property::WriteStructHeader(IFile &f,const StructDescription &strct)
{
	auto n = strct.GetMemberCount();
	if(n == 0)
		throw ImplementationError{"Attempted to write empty struct. This is not allowed!"};
	f.Write(n);
	f.Write(strct.types.data(),n *sizeof(Type));
	for(auto i=decltype(n){0u};i<n;++i)
		Write(f,strct.names[i]);
}
void udm::Property::Write(IFile &f,const Struct &strct)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	auto offsetToSize = WriteBlockSize<StructDescription::SizeType>(f);

	WriteStructHeader(f,strct.description);
	f.Write(strct.data.data(),strct.data.size());
	
	WriteBlockSize<StructDescription::SizeType>(f,offsetToSize);
}

void udm::Property::Write(IFile &f) const
{
	f.Write(type);
	if(is_non_trivial_type(type))
	{
		auto tag = get_non_trivial_tag(type);
		std::visit([&](auto tag){
			using T = decltype(tag)::type;
			Property::Write(f,*static_cast<const T*>(value));
		},tag);
		return;
	}
	f.Write(value,size_of(type));
}

udm::LinkedPropertyWrapper udm::Property::operator[](const std::string &key) {return LinkedPropertyWrapper{*this}[key];}
udm::LinkedPropertyWrapper udm::Property::operator[](const char *key) {return operator[](std::string{key});}

bool udm::Property::operator==(const Property &other) const
{
	auto res = (type == other.type);
	UDM_ASSERT_COMPARISON(res);
	if(!res)
		return false;
	if(is_trivial_type(type))
	{
		auto sz = size_of(type);
		res = (memcmp(value,other.value,sz) == 0);
		UDM_ASSERT_COMPARISON(res);
		return res;
	}
	auto tag = get_non_trivial_tag(type);
	return std::visit([this,&other](auto tag) -> bool {
		using T = decltype(tag)::type;
		auto res = (*static_cast<T*>(value) == *static_cast<T*>(other.value));
		UDM_ASSERT_COMPARISON(res);
		return res;
	},tag);
}

udm::BlobResult udm::Property::GetBlobData(const Blob &blob,void *outBuffer,size_t bufferSize)
{
	if(blob.data.size() != bufferSize)
		return BlobResult::InsufficientSize;
	memcpy(outBuffer,blob.data.data(),bufferSize);
	return BlobResult::Success;
}
udm::BlobResult udm::Property::GetBlobData(const BlobLz4 &blobLz4,void *outBuffer,size_t bufferSize)
{
	auto blob = decompress_lz4_blob(blobLz4);
	if(blob.data.size() != bufferSize)
		return BlobResult::InsufficientSize;
	memcpy(outBuffer,blob.data.data(),bufferSize);
	return BlobResult::Success;
}

udm::Blob udm::Property::GetBlobData(const BlobLz4 &blob) {return decompress_lz4_blob(blob);}

// udm::Property::operator udm::LinkedPropertyWrapper() {return LinkedPropertyWrapper{*this};}

bool udm::Property::Compress()
{
	switch(type)
	{
	case Type::Array:
	case Type::ArrayLz4:
	{
		auto &a = GetValue<Array>();
		if(!is_trivial_type(a.GetValueType()))
			return false;
		auto blobLz4 = udm::compress_lz4_blob(a.GetValues(),size_of(a.GetValueType()) *a.GetSize());
		Clear();
		type = Type::BlobLz4;
		auto *newBlob = new BlobLz4 {};
		*newBlob = std::move(blobLz4);
		value = newBlob;
		return true;
	}
	case Type::Blob:
	{
		auto blobLz4 = udm::compress_lz4_blob(GetValue<Blob>());
		Clear();
		type = Type::BlobLz4;
		auto *newBlob = new BlobLz4 {};
		*newBlob = std::move(blobLz4);
		value = newBlob;
		return true;
	}
	}
	return false;
}
bool udm::Property::Decompress(const std::optional<Type> arrayValueType)
{
	if(type != Type::BlobLz4)
		return false;
	if(!arrayValueType.has_value())
	{
		auto blob = udm::decompress_lz4_blob(GetValue<BlobLz4>());
		Clear();
		type = Type::Blob;
		auto *newBlob = new Blob {};
		*newBlob = std::move(blob);
		value = newBlob;
		return true;
	}
	if(!is_trivial_type(*arrayValueType))
		return false;
	auto &blobCompressed = GetValue<BlobLz4>();
	if((blobCompressed.uncompressedSize %size_of(*arrayValueType)) != 0)
		return false;
	auto *a = new Array {};
	a->SetValueType(*arrayValueType);
	auto numItems = blobCompressed.uncompressedSize /size_of(*arrayValueType);
	a->Resize(numItems);
	udm::decompress_lz4_blob(blobCompressed.compressedData.data(),blobCompressed.compressedData.size(),blobCompressed.uncompressedSize,a->GetValues());

	Clear();
	type = Type::Array;
	value = a;
	return true;
}

udm::BlobResult udm::Property::GetBlobData(void *outBuffer,size_t bufferSize,uint64_t *optOutRequiredSize) const
{
	if(!*this)
		return udm::BlobResult::InvalidProperty;
	switch(type)
	{
	case Type::Blob:
	{
		auto &blob = GetValue<Blob>();
		if(optOutRequiredSize)
			*optOutRequiredSize = blob.data.size();
		return GetBlobData(blob,outBuffer,bufferSize);
	}
	case Type::BlobLz4:
	{
		auto &blob = GetValue<BlobLz4>();
		if(optOutRequiredSize)
			*optOutRequiredSize = blob.uncompressedSize;
		return GetBlobData(blob,outBuffer,bufferSize);
	}
	case Type::Array:
	case Type::ArrayLz4:
	{
		auto &a = GetValue<Array>();
		auto byteSize = a.GetValueSize() *a.GetSize();
		if(optOutRequiredSize)
			*optOutRequiredSize = byteSize;
		if(bufferSize != byteSize)
			return BlobResult::InsufficientSize;
		auto valueType = a.GetValueType();
		if(is_non_trivial_type(valueType) && valueType != Type::Struct)
		{
			auto tag = get_non_trivial_tag(valueType);
			std::visit([this,outBuffer,&a](auto tag) {
				using T = decltype(tag)::type;
				auto n = a.GetSize();
				auto *srcPtr = static_cast<const T*>(a.GetValues());
				auto *dstPtr = static_cast<T*>(outBuffer);
				for(auto i=decltype(n){0u};i<n;++i)
				{
					*dstPtr = *srcPtr;
					++srcPtr;
					++dstPtr;
				}
			},tag);
		}
		else
			memcpy(outBuffer,a.GetValues(),bufferSize);
		return BlobResult::Success;
	}
	}
	return BlobResult::NotABlobType;
}

udm::BlobResult udm::Property::GetBlobData(void *outBuffer,size_t bufferSize,Type type,uint64_t *optOutRequiredSize) const
{
	auto result = GetBlobData(outBuffer,bufferSize,optOutRequiredSize);
	if(result != BlobResult::NotABlobType)
		return result;
	if(is_trivial_type(type))
	{
		if(is_array_type(this->type))
		{
			auto &a = GetValue<Array>();
			if(a.GetValueType() == type)
			{
				if(optOutRequiredSize)
					*optOutRequiredSize = a.GetSize() *size_of(a.GetValueType());
				if(a.GetSize() *size_of(a.GetValueType()) != bufferSize)
					return BlobResult::InsufficientSize;
				memcpy(outBuffer,a.GetValues(),bufferSize);
				return BlobResult::Success;
			}
		}
	}
	return BlobResult::NotABlobType;
}

udm::Blob udm::Property::GetBlobData(Type &outType) const
{
	outType = type;
	if(IsType(Type::Blob))
		return GetValue<Blob>();
	if(IsType(Type::BlobLz4))
		return decompress_lz4_blob(GetValue<BlobLz4>());
	if(is_trivial_type(type))
	{
		if(is_array_type(this->type))
		{
			auto &a = GetValue<Array>();
			udm::Blob blob {};
			blob.data.resize(a.GetSize() *size_of(a.GetValueType()));
			memcpy(blob.data.data(),a.GetValues(),blob.data.size());
			return blob;
		}
	}
	return {};
}

void *udm::Property::GetValuePtr(Type &outType)
{
	outType = this->type;
	return value;
}

bool udm::Property::Read(Type ptype,IFile &f)
{
	Clear();
	type = ptype;
	Initialize();

	if(is_non_trivial_type(type))
	{
		auto tag = get_non_trivial_tag(type);
		return std::visit([this,&f](auto tag){return Read(f,*static_cast<decltype(tag)::type*>(value));},tag);
	}
	auto size = size_of(type);
	value = new uint8_t[size];
	f.Read(value,size);
	return true;
}
bool udm::Property::Read(IFile &f) {return Read(f.Read<Type>(),f);}

void udm::Property::ToAscii(AsciiSaveFlags flags,std::stringstream &ss,const std::string &propName,Type type,const DataValue value,const std::string &prefix)
{
	if(type == Type::Element)
	{
		ss<<prefix<<'\"'<<propName<<"\"\n";
		ss<<prefix<<"{\n";
		static_cast<Element*>(value)->ToAscii(flags,ss,prefix);
		ss<<'\n'<<prefix<<"}";
		return;
	}
	if(is_numeric_type(type))
	{
		auto tag = get_numeric_tag(type);
		std::visit([value,&ss](auto tag){
			using T = decltype(tag)::type;
			NumericTypeToString(*static_cast<T*>(value),ss);
		},tag);
	}
	else
	{
		auto vs = [value,&ss,&prefix,flags](auto tag){
			using T = decltype(tag)::type;
			ss<<ToAsciiValue(flags,*static_cast<T*>(value),prefix);
		};
		if(is_gnt_type(type))
			visit_gnt(type,vs);
	}
}

void udm::Property::ToAscii(AsciiSaveFlags flags,std::stringstream &ss,const std::string &propName,const std::string &prefix)
{
	if(type == Type::Element)
	{
		ss<<prefix<<'\"'<<propName<<"\"\n";
		ss<<prefix<<"{\n";
		static_cast<Element*>(value)->ToAscii(flags,ss,prefix);
		ss<<'\n'<<prefix<<"}";
		return;
	}
	ss<<prefix<<"$"<<enum_type_to_ascii(type);
	if(type == Type::Struct)
		ss<<(*static_cast<Struct*>(value))->GetTemplateArgumentList();
	ss<<" ";
	if(does_key_require_quotes(propName))
		ss<<'\"'<<propName<<'\"';
	else
		ss<<propName;
	ss<<" ";
	if(is_numeric_type(type))
	{
		auto tag = get_numeric_tag(type);
		std::visit([this,&ss](auto tag){
			using T = decltype(tag)::type;
			NumericTypeToString(*static_cast<T*>(value),ss);
		},tag);
	}
	else
	{
		auto vs = [this,&ss,&prefix,flags](auto tag){
			using T = decltype(tag)::type;
			ss<<ToAsciiValue(flags,*static_cast<T*>(value),prefix);
		};
		if(is_gnt_type(type))
			visit_gnt(type,vs);
	}
}
