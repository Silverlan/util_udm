/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sharedutils/base64.hpp>
#include <sstream>

#pragma optimize("",off)
udm::PProperty udm::Property::Create(Type type)
{
	auto prop = std::shared_ptr<Property>{new Property{}};
	prop->type = type;
	prop->Initialize();
	if(type == Type::Element)
		static_cast<Element*>(prop->value)->fromProperty = *prop;
	else if(type == Type::Array)
		static_cast<Array*>(prop->value)->fromProperty = *prop;
	return prop;
}

udm::Property::Property(Property &&other)
{
	type = other.type;
	value = other.value;

	other.type = Type::Nil;
	other.value = nullptr;
}

udm::Property::~Property() {Clear();}

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

bool udm::Property::Read(const VFilePtr &f,Blob &outBlob)
{
	auto size = f->Read<size_t>();
	outBlob.data.resize(size);
	f->Read(outBlob.data.data(),size);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,BlobLz4 &outBlob)
{
	auto compressedSize = f->Read<size_t>();
	auto uncompressedSize = f->Read<size_t>();
	outBlob.uncompressedSize = uncompressedSize;
	outBlob.compressedData.resize(compressedSize);
	f->Read(outBlob.compressedData.data(),compressedSize);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,Utf8String &outStr)
{
	auto size = f->Read<uint32_t>();
	outStr.data.resize(size);
	f->Read(outStr.data.data(),size);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,Element &el)
{
	f->Seek(f->Tell() +sizeof(uint64_t)); // Skip size

	auto numChildren = f->Read<uint32_t>();
	std::vector<std::string> stringTable {};
	stringTable.resize(numChildren);
	for(auto i=decltype(numChildren){0u};i<numChildren;++i)
		stringTable[i] = Data::ReadKey(f);
	el.children.reserve(numChildren);
	for(auto i=decltype(numChildren){0u};i<numChildren;++i)
	{
		auto &name = stringTable[i];
		auto prop = Property::Create();
		if(prop->Read(f)== false)
			return false;
		el.children[std::move(name)] = prop;
	}
	return true;
}
bool udm::Property::Read(const VFilePtr &f,String &outStr)
{
	uint32_t len = f->Read<uint8_t>();
	if(len == EXTENDED_STRING_IDENTIFIER)
		len = f->Read<uint32_t>();
	outStr.resize(len);
	f->Read(outStr.data(),len);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,Reference &outRef)
{
	String str;
	if(Read(f,str) == false)
		return false;
	outRef.path = std::move(str);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,Array &a)
{
	a.Clear();
	a.valueType = f->Read<decltype(a.valueType)>();
	a.size = f->Read<decltype(a.size)>();
	a.fromProperty = {*this};
	if(is_non_trivial_type(a.valueType))
	{
		f->Seek(f->Tell() +sizeof(uint64_t)); // Skip size
		auto tag = get_non_trivial_tag(a.valueType);
		return std::visit([this,&f,&a](auto tag) {
			using T = decltype(tag)::type;
			a.values = new T[a.size];
			for(auto i=decltype(a.size){0u};i<a.size;++i)
			{
				if(Read(f,static_cast<T*>(a.values)[i]) == false)
					return false;
			}

			// Also see udm::Array::Resize
			if constexpr(std::is_same_v<T,Element> || std::is_same_v<T,Array>)
			{
				auto *p = static_cast<T*>(a.values);
				for(auto i=decltype(a.size){0u};i<a.size;++i)
					p[i].fromProperty = PropertyWrapper{a,i};
			}
			return true;
		},tag);
	}

	auto size = a.size *size_of(a.valueType);
	a.values = new uint8_t[size];
	f->Read(a.values,size);
	return true;
}

std::string udm::Property::ToAsciiValue(const Nil &nil,const std::string &prefix) {return "";}
std::string udm::Property::ToAsciiValue(const Blob &blob,const std::string &prefix)
{
	try
	{
		return util::base64_encode(blob.data.data(),blob.data.size());
	}
	catch(const std::runtime_error &e)
	{
		throw CompressionError{e.what()};
	}
	return "";
}
std::string udm::Property::ToAsciiValue(const BlobLz4 &blob,const std::string &prefix)
{
	try
	{
		return "[" +std::to_string(blob.uncompressedSize) +"][" +util::base64_encode(blob.compressedData.data(),blob.compressedData.size()) +"]";
	}
	catch(const std::runtime_error &e)
	{
		throw CompressionError{e.what()};
	}
	return "";
}
std::string udm::Property::ToAsciiValue(const Utf8String &utf8,const std::string &prefix)
{
	try
	{
		return "[base64][" +util::base64_encode(utf8.data.data(),utf8.data.size()) +']';
	}
	catch(const std::runtime_error &e)
	{
		throw CompressionError{e.what()};
	}
	return "";
}
std::string udm::Property::ToAsciiValue(const Element &el,const std::string &prefix)
{
	// Unreachable
	throw std::runtime_error{"Cannot convert value of type Element to ASCII!"};
}
std::string udm::Property::ToAsciiValue(const Array &a,const std::string &prefix)
{
	std::stringstream ss;
	ss<<"[";
	ss<<enum_type_to_ascii(a.valueType)<<';'<<a.GetSize();
	ss<<"][";

	if(a.valueType == Type::Element)
	{
		auto *ptr = static_cast<Element*>(a.values);
		for(auto i=decltype(a.size){0u};i<a.size;++i)
		{
			if(i > 0)
				ss<<',';
			ss<<"\n";
			ss<<prefix<<"\t{\n";
			ptr->ToAscii(ss,prefix +"\t");
			ss<<"\n"<<prefix<<"\t}";
			++ptr;
		}
		if(a.size > 0)
			ss<<"\n"<<prefix;
	}
	else if(is_numeric_type(a.valueType))
	{
		auto tag = get_numeric_tag(a.valueType);
		std::visit([&a,&ss](auto tag) {
			using T = decltype(tag)::type;
			auto *ptr = static_cast<T*>(a.values);
			for(auto i=decltype(a.size){0u};i<a.size;++i)
			{
				if(i > 0)
					ss<<",";
				NumericTypeToString(*ptr,ss);
				++ptr;
			}
		},tag);
	}
	else
	{
		auto vs = [&a,&ss,&prefix](auto tag){
			using T = decltype(tag)::type;
			auto *ptr = static_cast<T*>(a.values);
			for(auto i=decltype(a.size){0u};i<a.size;++i)
			{
				if(i > 0)
					ss<<",";
				ss<<ToAsciiValue(*ptr,prefix);
				++ptr;
			}
		};
		if(is_generic_type(a.valueType))
			std::visit(vs,get_generic_tag(a.valueType));
		else if(is_non_trivial_type(a.valueType))
			std::visit(vs,get_non_trivial_tag(a.valueType));
	}
	ss<<"]";
	return ss.str();
}
std::string udm::Property::ToAsciiValue(const String &str,const std::string &prefix)
{
	auto val = str;
	ustring::replace(val,"\\","\\\\");
	return '\"' +val +'\"';
}
std::string udm::Property::ToAsciiValue(const Reference &ref,const std::string &prefix)
{
	return ToAsciiValue(ref.path,prefix);
}
		
std::string udm::Property::ToAsciiValue(const Vector2 &v,const std::string &prefix) {return '[' +std::to_string(v.x) +',' +std::to_string(v.y) +']';}
std::string udm::Property::ToAsciiValue(const Vector3 &v,const std::string &prefix) {return '[' +std::to_string(v.x) +',' +std::to_string(v.y) +',' +std::to_string(v.z) +']';}
std::string udm::Property::ToAsciiValue(const Vector4 &v,const std::string &prefix) {return '[' +std::to_string(v.x) +',' +std::to_string(v.y) +',' +std::to_string(v.z) +',' +std::to_string(v.w) +']';}
std::string udm::Property::ToAsciiValue(const Quaternion &q,const std::string &prefix) {return '[' +std::to_string(q.w) +',' +std::to_string(q.x) +',' +std::to_string(q.y) +',' +std::to_string(q.z) +']';}
std::string udm::Property::ToAsciiValue(const EulerAngles &a,const std::string &prefix) {return '[' +std::to_string(a.p) +',' +std::to_string(a.y) +',' +std::to_string(a.r) +']';}
std::string udm::Property::ToAsciiValue(const Srgba &srgb,const std::string &prefix) {return '[' +std::to_string(srgb[0]) +',' +std::to_string(srgb[1]) +',' +std::to_string(srgb[2]) +',' +std::to_string(srgb[3]) +']';}
std::string udm::Property::ToAsciiValue(const HdrColor &col,const std::string &prefix) {return '[' +std::to_string(col[0]) +',' +std::to_string(col[1]) +',' +std::to_string(col[2]) +']';}
std::string udm::Property::ToAsciiValue(const Transform &t,const std::string &prefix)
{
	auto &pos = t.GetOrigin();
	auto &rot = t.GetRotation();
	std::string s = "[";
	s += "[" +std::to_string(pos.x) +',' +std::to_string(pos.y) +',' +std::to_string(pos.z) +"]";
	s += "[" +std::to_string(rot.w) +',' +std::to_string(rot.x) +',' +std::to_string(rot.y) +',' +std::to_string(rot.z) +"]";
	s += "]";
	return s;
}
std::string udm::Property::ToAsciiValue(const ScaledTransform &t,const std::string &prefix)
{
	auto &pos = t.GetOrigin();
	auto &rot = t.GetRotation();
	auto &scale = t.GetScale();
	std::string s = "[";
	s += "[" +std::to_string(pos.x) +',' +std::to_string(pos.y) +',' +std::to_string(pos.z) +"]";
	s += "[" +std::to_string(rot.w) +',' +std::to_string(rot.x) +',' +std::to_string(rot.y) +',' +std::to_string(rot.z) +"]";
	s += "[" +std::to_string(scale.x) +',' +std::to_string(scale.y) +',' +std::to_string(scale.z) +"]";
	s += "]";
	return s;
}
std::string udm::Property::ToAsciiValue(const Mat4 &m,const std::string &prefix)
{
	std::string s {"["};
	for(uint8_t i=0;i<4;++i)
	{
		s += '[';
		for(uint8_t j=0;j<4;++j)
		{
			if(i > 0 || j > 0)
				s += ',';
			s += std::to_string(m[i][j]);
		}
		s += ']';
	}
	s += "]";
	return s;
}
std::string udm::Property::ToAsciiValue(const Mat3x4 &m,const std::string &prefix)
{
	std::string s {"["};
	for(uint8_t i=0;i<3;++i)
	{
		s += '[';
		for(uint8_t j=0;j<4;++j)
		{
			if(i > 0 || j > 0)
				s += ',';
			s += std::to_string(m[i][j]);
		}
		s += ']';
	}
	s += "]";
	return s;
}

void udm::Property::Write(VFilePtrReal &f,const Blob &blob)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f->Write<size_t>(blob.data.size());
	f->Write(blob.data.data(),blob.data.size());
}
void udm::Property::Write(VFilePtrReal &f,const BlobLz4 &blob)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f->Write<size_t>(blob.compressedData.size());
	f->Write<size_t>(blob.uncompressedSize);
	f->Write(blob.compressedData.data(),blob.compressedData.size());
}
void udm::Property::Write(VFilePtrReal &f,const Utf8String &str)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f->Write<uint32_t>(str.data.size());
	f->Write(str.data.data(),str.data.size());
}
void udm::Property::Write(VFilePtrReal &f,const Element &el)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	auto offsetToSize = f->Tell();
	f->Write<uint64_t>(0);

	f->Write<uint32_t>(el.children.size());
	for(auto &pair : el.children)
		Data::WriteKey(f,pair.first);

	for(auto &pair : el.children)
		pair.second->Write(f);

	auto startOffset = offsetToSize +sizeof(uint64_t);
	auto curOffset = f->Tell();
	f->Seek(offsetToSize);
	f->Write<uint64_t>(curOffset -startOffset);
	f->Seek(curOffset);
}
void udm::Property::Write(VFilePtrReal &f,const Array &a)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	f->Write(a.valueType);
	f->Write(a.size);
	if(is_non_trivial_type(a.valueType))
	{
		auto offsetToSize = f->Tell();
		f->Write<uint64_t>(0);

		auto tag = get_non_trivial_tag(a.valueType);
		std::visit([&](auto tag){
			using T = decltype(tag)::type;
			for(auto i=decltype(a.size){0u};i<a.size;++i)
				Property::Write(f,static_cast<T*>(a.values)[i]);
		},tag);

		auto startOffset = offsetToSize +sizeof(uint64_t);
		auto curOffset = f->Tell();
		f->Seek(offsetToSize);
		f->Write<uint64_t>(curOffset -startOffset);
		f->Seek(curOffset);
		return;
	}
	f->Write(a.values,a.size *size_of(a.valueType));
}
void udm::Property::Write(VFilePtrReal &f,const String &str)
{
	// Note: Any changes made here may affect udm::Data::SkipProperty as well
	auto len = umath::min(str.length(),static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
	if(len < EXTENDED_STRING_IDENTIFIER)
		f->Write<uint8_t>(len);
	else
	{
		f->Write<uint8_t>(EXTENDED_STRING_IDENTIFIER);
		f->Write<uint32_t>(len);
	}
	f->Write(str.data(),len);
}
void udm::Property::Write(VFilePtrReal &f,const Reference &ref)
{
	Write(f,ref.path);
}

void udm::Property::Write(VFilePtrReal &f) const
{
	f->Write(type);
	if(is_non_trivial_type(type))
	{
		auto tag = get_non_trivial_tag(type);
		std::visit([&](auto tag){
			using T = decltype(tag)::type;
			Property::Write(f,*static_cast<const T*>(value));
		},tag);
		return;
	}
	f->Write(value,size_of(type));
}

udm::LinkedPropertyWrapper udm::Property::operator[](const std::string &key) {return LinkedPropertyWrapper{*this}[key];}
udm::LinkedPropertyWrapper udm::Property::operator[](const char *key) {return operator[](std::string{key});}

bool udm::Property::operator==(const Property &other) const
{
	if(type != other.type)
		return false;
	if(is_trivial_type(type))
	{
		auto sz = size_of(type);
		return memcmp(value,other.value,sz) == 0;
	}
	auto tag = get_non_trivial_tag(type);
	return std::visit([this,&other](auto tag) -> bool {
		using T = decltype(tag)::type;
		return *static_cast<T*>(value) == *static_cast<T*>(other.value);
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
	{
		auto &a = GetValue<Array>();
		auto byteSize = a.GetSize() *size_of_base_type(a.valueType);
		if(optOutRequiredSize)
			*optOutRequiredSize = byteSize;
		if(bufferSize != byteSize)
			return BlobResult::InsufficientSize;
		if(is_non_trivial_type(a.valueType))
		{
			auto tag = get_non_trivial_tag(a.valueType);
			std::visit([this,outBuffer,&a](auto tag) {
				using T = decltype(tag)::type;
				auto n = a.GetSize();
				auto *srcPtr = static_cast<T*>(a.values);
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
			memcpy(outBuffer,a.values,bufferSize);
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
		if(IsType(Type::Array))
		{
			auto &a = GetValue<Array>();
			if(a.valueType == type)
			{
				if(optOutRequiredSize)
					*optOutRequiredSize = a.GetSize() *size_of(a.valueType);
				if(a.GetSize() *size_of(a.valueType) != bufferSize)
					return BlobResult::InsufficientSize;
				memcpy(outBuffer,a.values,bufferSize);
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
		if(IsType(Type::Array))
		{
			auto &a = GetValue<Array>();
			udm::Blob blob {};
			blob.data.resize(a.GetSize() *size_of(a.valueType));
			memcpy(blob.data.data(),a.values,blob.data.size());
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

bool udm::Property::Read(Type ptype,const VFilePtr &f)
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
	f->Read(value,size);
	return true;
}
bool udm::Property::Read(const VFilePtr &f) {return Read(f->Read<Type>(),f);}

void udm::Property::ToAscii(std::stringstream &ss,const std::string &propName,Type type,const DataValue value,const std::string &prefix)
{
	if(type == Type::Element)
	{
		ss<<prefix<<'\"'<<propName<<"\"\n";
		ss<<prefix<<"{\n";
		static_cast<Element*>(value)->ToAscii(ss,prefix);
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
		auto vs = [value,&ss,&prefix](auto tag){
			using T = decltype(tag)::type;
			ss<<ToAsciiValue(*static_cast<T*>(value),prefix);
		};
		if(is_generic_type(type))
			std::visit(vs,get_generic_tag(type));
		else if(is_non_trivial_type(type))
			std::visit(vs,get_non_trivial_tag(type));
	}
}

void udm::Property::ToAscii(std::stringstream &ss,const std::string &propName,const std::string &prefix)
{
	if(type == Type::Element)
	{
		ss<<prefix<<'\"'<<propName<<"\"\n";
		ss<<prefix<<"{\n";
		static_cast<Element*>(value)->ToAscii(ss,prefix);
		ss<<'\n'<<prefix<<"}";
		return;
	}
	ss<<prefix<<"$"<<enum_type_to_ascii(type)<<" ";
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
		auto vs = [this,&ss,&prefix](auto tag){
			using T = decltype(tag)::type;
			ss<<ToAsciiValue(*static_cast<T*>(value),prefix);
		};
		if(is_generic_type(type))
			std::visit(vs,get_generic_tag(type));
		else if(is_non_trivial_type(type))
			std::visit(vs,get_non_trivial_tag(type));
	}
}
#pragma optimize("",on)
