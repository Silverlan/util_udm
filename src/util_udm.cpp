/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/base64.hpp>
#include <sstream>
#pragma optimize("",off)

static void test()
{
	udm::is_convertible<float,std::string>();
	udm::is_convertible<float>(udm::Type::String);
	udm::is_convertible_from<float>(udm::Type::String);
	udm::Property prop {};
	prop.GetValue<float>();
	prop.ToValue<float>();
	udm::Property::Create<float>(5.f);
	udm::Blob data {};
	udm::Property::Create(data);
	udm::Property::Create(std::move(data));

	udm::PropertyWrapper wrapper {};
	wrapper = 5.f;
	wrapper = std::vector<float>{1.f,2.f,3.f};
	wrapper = std::vector<std::string>{};

	udm::Array a;
	a[0] = 5.f;
	auto b = a[1];
	auto f = static_cast<float>(b);
}

udm::Type udm::ascii_type_to_enum(const char *type)
{
	// Note: These have to match enum_type_to_ascii
	static std::unordered_map<const char*,Type> namedTypeToEnum = {
		{"nil",Type::Nil},
		{"string",Type::String},
		{"utf8",Type::Utf8String},
		{"int8",Type::Int8},
		{"uint8",Type::UInt8},
		{"int16",Type::Int16},
		{"uint16",Type::UInt16},
		{"int32",Type::Int32},
		{"uint32",Type::UInt32},
		{"int64",Type::Int64},
		{"uint64",Type::UInt64},
		{"float",Type::Float},
		{"double",Type::Double},
		{"bool",Type::Boolean},
		{"vec2",Type::Vector2},
		{"vec3",Type::Vector3},
		{"vec4",Type::Vector4},
		{"quat",Type::Quaternion},
		{"ang",Type::EulerAngles},
		{"srgba",Type::Srgba},
		{"hdr",Type::HdrColor},
		{"transform",Type::Transform},
		{"stransform",Type::ScaledTransform},
		{"mat4",Type::Mat4},
		{"mat3x4",Type::Mat3x4},
		{"blob",Type::Blob},
		{"lz4",Type::BlobLz4},
		{"array",Type::Array},
		{"element",Type::Element}
	};
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	auto it = namedTypeToEnum.find(type);
	return (it != namedTypeToEnum.end()) ? it->second : Type::Nil;
}

void udm::sanitize_key_name(std::string &key)
{
	ustring::replace(key,".","");
}

udm::LinkedPropertyWrapper udm::Element::AddArray(const std::string_view &path,std::optional<uint32_t> size,Type type)
{
	auto prop = Add(path,Type::Array);
	if(!prop)
		return prop;
	auto &a = *static_cast<Array*>(prop->value);
	a.SetValueType(type);
	if(size.has_value())
		a.Resize(*size);
	return prop;
}

udm::LinkedPropertyWrapper udm::Element::Add(const std::string_view &path,Type type)
{
	auto end = path.find('.');
	auto name = path.substr(0,end);
	if(name.empty())
		return fromProperty;
	std::string strName {name};
	auto isLast = (end == std::string::npos);
	auto it = children.find(strName);
	if(isLast && it != children.end() && it->second->type != type)
		children.erase(it);
	if(it == children.end())
	{
		if(isLast)
			AddChild(strName,Property::Create(type));
		else
			AddChild(strName,Property::Create<Element>());
		it = children.find(strName);
		assert(it != children.end());
	}
	if(isLast)
		return *it->second;
	return static_cast<Element*>(it->second->value)->Add(path.substr(end +1),type);
}

std::shared_ptr<udm::Data> udm::Data::Load(const std::string &fileName,std::string &outErr)
{
	auto f = FileManager::OpenFile(fileName.c_str(),"rb");
	if(f == nullptr)
	{
		outErr = "Unable to open file!";
		return nullptr;
	}
	return Load(f,outErr);
}

bool udm::Data::Save(const std::string &fileName,std::string &outErr) const
{
	auto f = FileManager::OpenFile<VFilePtrReal>(fileName.c_str(),"wb");
	if(f == nullptr)
	{
		outErr = "Unable to open file!";
		return nullptr;
	}
	return Save(f,outErr);
}

void udm::Data::WriteProperty(VFilePtrReal &f,const Property &o) {o.Write(f);}

udm::PProperty udm::Data::ReadProperty(const VFilePtr &f,std::string &outErr)
{
	auto prop = Property::Create();
	if(prop->Read(f,outErr) == false)
		return nullptr;
	return prop;
}

bool udm::Property::Read(const VFilePtr &f,Blob &outBlob,std::string &outErr)
{
	auto size = f->Read<size_t>();
	outBlob.data.resize(size);
	f->Read(outBlob.data.data(),size);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,BlobLz4 &outBlob,std::string &outErr)
{
	auto compressedSize = f->Read<size_t>();
	auto uncompressedSize = f->Read<size_t>();
	outBlob.uncompressedSize = uncompressedSize;
	outBlob.compressedData.resize(compressedSize);
	f->Read(outBlob.compressedData.data(),compressedSize);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,Utf8String &outStr,std::string &outErr)
{
	auto size = f->Read<uint32_t>();
	outStr.data.resize(size);
	f->Read(outStr.data.data(),size);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,Element &el,std::string &outErr)
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
		if(prop->Read(f,outErr)== false)
			return false;
		el.children[std::move(name)] = prop;
	}
	return true;
}
bool udm::Property::Read(const VFilePtr &f,String &outStr,std::string &outErr)
{
	uint32_t len = f->Read<uint8_t>();
	if(len == EXTENDED_STRING_IDENTIFIER)
		len = f->Read<uint32_t>();
	outStr.resize(len);
	f->Read(outStr.data(),len);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,Array &a,std::string &outErr)
{
	a.Clear();
	a.valueType = f->Read<decltype(a.valueType)>();
	a.size = f->Read<decltype(a.size)>();
	a.fromProperty = {*this};
	if(is_non_trivial_type(a.valueType))
	{
		f->Seek(f->Tell() +sizeof(uint64_t)); // Skip size
		auto tag = get_non_trivial_tag(a.valueType);
		return std::visit([this,&f,&a,&outErr](auto tag) {
			using T = decltype(tag)::type;
			a.values = new T[a.size];
			for(auto i=decltype(a.size){0u};i<a.size;++i)
			{
				if(Read(f,static_cast<T*>(a.values)[i],outErr) == false)
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
std::string udm::Property::ToAsciiValue(const Blob &blob,const std::string &prefix) {return util::base64_encode(blob.data.data(),blob.data.size());}
std::string udm::Property::ToAsciiValue(const BlobLz4 &blob,const std::string &prefix) {return "[" +std::to_string(blob.uncompressedSize) +"][" +util::base64_encode(blob.compressedData.data(),blob.compressedData.size()) +"]";}
std::string udm::Property::ToAsciiValue(const Utf8String &utf8,const std::string &prefix) {return "[base64][" +util::base64_encode(utf8.data.data(),utf8.data.size()) +']';}
std::string udm::Property::ToAsciiValue(const Element &el,const std::string &prefix) {throw std::runtime_error{"Cannot convert value of type Element to ASCII!"};}
std::string udm::Property::ToAsciiValue(const Array &a,const std::string &prefix)
{
	std::stringstream ss;
	ss<<"[";
	ss<<enum_type_to_ascii(a.valueType);
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
std::string udm::Property::ToAsciiValue(const String &str,const std::string &prefix) {return '\"' +str +'\"';}
		
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

udm::BlobResult udm::Property::GetBlobData(void *outBuffer,size_t bufferSize,uint64_t *optOutRequiredSize) const
{
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

bool udm::Property::Read(Type ptype,const VFilePtr &f,std::string &outErr)
{
	Clear();
	type = ptype;
	Initialize();

	if(is_non_trivial_type(type))
	{
		auto tag = get_non_trivial_tag(type);
		return std::visit([this,&f,&outErr](auto tag){return Read(f,*static_cast<decltype(tag)::type*>(value),outErr);},tag);
	}
	auto size = size_of(type);
	value = new uint8_t[size];
	f->Read(value,size);
	return true;
}
bool udm::Property::Read(const VFilePtr &f,std::string &outErr) {return Read(f->Read<Type>(),f,outErr);}

std::shared_ptr<udm::Data> udm::Data::Create(const std::string &assetType,Version assetVersion,std::string &outErr)
{
	auto udmData = std::shared_ptr<udm::Data>{new udm::Data{}};
	udmData->m_rootProperty = Property::Create<Element>();
	udmData->SetAssetType(assetType);
	udmData->SetAssetVersion(assetVersion);
	udmData->m_rootProperty->GetValue<Element>().Add(KEY_ASSET_DATA);
	return udmData;
}

std::shared_ptr<udm::Data> udm::Data::Open(const std::string &fileName,std::string &outErr)
{
	auto f = FileManager::OpenFile(fileName.c_str(),"rb");
	if(f == nullptr)
	{
		outErr = "Unable to open file!";
		return nullptr;
	}
	return Open(f,outErr);
}
std::shared_ptr<udm::Data> udm::Data::Open(const VFilePtr &f,std::string &outErr)
{
	auto udmData = std::shared_ptr<udm::Data>{new udm::Data{}};
	udmData->m_header = f->Read<Header>();
	if(ustring::compare(udmData->m_header.identifier.data(),HEADER_IDENTIFIER,true,strlen(HEADER_IDENTIFIER)) == false)
	{
		outErr = "Unexpected header identifier, file is not a valid UDM file!";
		return nullptr;
	}
	if(udmData->m_header.version == 0)
	{
		outErr = "Unexpected header version, file is not a valid UDM file!";
		return nullptr;
	}
	if(udmData->m_header.version > VERSION)
	{
		outErr = "File uses a newer UDM version (" +std::to_string(udmData->m_header.version) +") than is supported by this version of UDM (" +std::to_string(VERSION) +")!";
		return nullptr;
	}
	udmData->m_file = f;
	return udmData;
}

bool udm::Data::ValidateHeaderProperties(std::string &outErr)
{
	auto &el = m_rootProperty->GetValue<Element>();
	constexpr std::array<const char*,3> requiredKeys = {KEY_ASSET_TYPE,KEY_ASSET_VERSION,KEY_ASSET_DATA};
	constexpr std::array<Type,3> requiredKeyTypes = {Type::String,Type::UInt32,Type::Element};
	for(auto i=decltype(requiredKeys.size()){0u};i<requiredKeys.size();++i)
	{
		auto it = el.children.find(requiredKeys[i]);
		if(it != el.children.end())
		{
			if(it->second->type != requiredKeyTypes[i])
			{
				outErr = "Excepted type " +std::string{magic_enum::enum_name(requiredKeyTypes[i])} +" for KeyValue '" +std::string{requiredKeys[i]} +"', but got type " +std::string{magic_enum::enum_name(it->second->type)} +"!";
				return false;
			}
			continue;
		}
		outErr = "KeyValue '" +std::string{requiredKeys[i]} +"' not found! Not a valid UDM file!";
		return false;
	}
	return true;
}

std::shared_ptr<udm::Data> udm::Data::Load(const VFilePtr &f,std::string &outErr)
{
	auto udmData = Open(f,outErr);
	if(udmData == nullptr)
		return nullptr;
	auto o = ReadProperty(f,outErr);
	if(o == nullptr)
	{
		outErr = "Root element is invalid!";
		return nullptr;
	}
	if(o->type != Type::Element)
	{
		outErr = "Expected root element to be type Element, but is type " +std::string{magic_enum::enum_name(o->type)} +"!";
		return nullptr;
	}
	udmData->m_rootProperty = o;
	udmData->m_file = nullptr; // Don't need the file handle anymore
	return udmData->ValidateHeaderProperties(outErr) ? udmData : nullptr;
}

udm::PProperty udm::Data::LoadProperty(const std::string_view &path,std::string &outErr) const
{
	auto f = m_file;
	if(f == nullptr)
	{
		outErr = "Invalid file handle!";
		return nullptr;
	}
	f->Seek(sizeof(m_header));
	auto type = f->Read<Type>();
	return LoadProperty(type,std::string{KEY_ASSET_DATA} +"." +std::string{path},outErr);
}

void udm::Data::SkipProperty(VFilePtr &f,Type type)
{
	if(is_numeric_type(type) || is_generic_type(type))
	{
		f->Seek(f->Tell() +size_of(type));
		return;
	}
	switch(type)
	{
	case Type::String:
	{
		uint32_t len = f->Read<uint8_t>();
		if(len == Property::EXTENDED_STRING_IDENTIFIER)
			len = f->Read<uint32_t>();
		f->Seek(f->Tell() +len);
		break;
	}
	case Type::Utf8String:
	{
		auto size = f->Read<uint32_t>();
		f->Seek(f->Tell() +size);
		break;
	}
	case Type::Blob:
	{
		auto size = f->Read<size_t>();
		f->Seek(f->Tell() +size);
		break;
	}
	case Type::BlobLz4:
	{
		auto compressedSize = f->Read<size_t>();
		f->Seek(f->Tell() +sizeof(size_t) +compressedSize);
		break;
	}
	case Type::Array:
	{
		auto valueType = f->Read<Type>();
		if(is_non_trivial_type(valueType))
		{
			f->Seek(f->Tell() +sizeof(decltype(Array::size)));
			auto sizeBytes = f->Read<uint64_t>();
			f->Seek(f->Tell() +sizeBytes);
			break;
		}
		auto size = f->Read<decltype(Array::size)>();
		auto n = f->Read<decltype(Array::size)>();
		f->Seek(f->Tell() +n *size_of(valueType));
		break;
	}
	case Type::Element:
	{
		auto size = f->Read<uint64_t>();
		f->Seek(f->Tell() +size);
		break;
	}
	}
	static_assert(NON_TRIVIAL_TYPES.size() == 6);
}

std::string udm::Data::ReadKey(const VFilePtr &f)
{
	auto len = f->Read<uint8_t>();
	std::string str;
	str.resize(len);
	f->Read(str.data(),len);
	return str;
}
void udm::Data::WriteKey(VFilePtrReal &f,const std::string &key)
{
	if(key.length() > std::numeric_limits<uint8_t>::max())
		return WriteKey(f,key.substr(0,std::numeric_limits<uint8_t>::max()));
	f->Write<uint8_t>(key.length());
	f->Write(key.data(),key.length());
}

udm::PProperty udm::Data::LoadProperty(Type type,const std::string_view &path,std::string &outErr) const
{
	auto end = path.find('.');
	auto name = path.substr(0,end);
	if(name.empty())
	{
		outErr = "Invalid name!";
		return nullptr;
	}

	auto f = m_file;
	if(f == nullptr)
	{
		outErr = "Invalid file handle!";
		return nullptr;
	}
	auto isLast = (end == std::string::npos);
	
	if(type != Type::Element)
	{
		if(type == Type::Array)
		{
			std::string str{name};
			if(ustring::is_integer(str) == false)
			{
				outErr = "Attempted to retrieve non-integer index from array type, which is not allowed!";
				return nullptr;
			}
			auto i = ustring::to_int(str);
			auto valueType = f->Read<Type>();
			auto n = f->Read<decltype(Array::size)>();
			if(i >= n || i < 0)
			{
				outErr = "Array index out of bounds!";
				return nullptr;
			}
			
			if(is_trivial_type(valueType))
			{
				if(!isLast)
				{
					outErr = "Non-trailing property '" +std::string{name} +"[" +str +"]' is of type " +std::string{magic_enum::enum_name(valueType)} +", but " +std::string{magic_enum::enum_name(Type::Element)} +" expected!";
					return nullptr;
				}
				f->Seek(f->Tell() +i *size_of(valueType));
				auto prop = Property::Create();
				if(prop->Read(valueType,f,outErr) == false)
					return nullptr;
				return prop;
			}

			f->Seek(f->Tell() +sizeof(uint64_t));
			for(auto j=decltype(i){0u};j<i;++j)
				SkipProperty(f,valueType);

			if(isLast)
			{
				auto prop = Property::Create();
				if(prop->Read(valueType,f,outErr) == false)
					return nullptr;
				return prop;
			}
			return LoadProperty(valueType,path.substr(end +1),outErr);
		}
		outErr = "Non-trailing property '" +std::string{name} +"' is of type " +std::string{magic_enum::enum_name(type)} +", but " +std::string{magic_enum::enum_name(Type::Element)} +" expected!";
		return nullptr;
	}

	auto elStartOffset = f->Tell();
	f->Seek(f->Tell() +sizeof(uint64_t));
	auto numChildren = f->Read<uint32_t>();
	uint32_t ichild = std::numeric_limits<uint32_t>::max();
	for(auto i=decltype(numChildren){0u};i<numChildren;++i)
	{
		auto str = ReadKey(f);
		if(str == name)
			ichild = i;
	}
	if(ichild == std::numeric_limits<uint32_t>::max())
	{
		outErr = "Element with specified name not found!";
		return nullptr;
	}
	// Skip all children until we get the the one we want
	for(auto i=decltype(ichild){0u};i<ichild;++i)
		SkipProperty(f,f->Read<Type>());

	if(isLast)
		return ReadProperty(f,outErr); // Read this property in full
	auto childType = f->Read<Type>();
	return LoadProperty(childType,path.substr(end +1),outErr);
}

std::string udm::AssetData::GetAssetType() const
{
	auto prop = (*this)["assetType"];
	auto *val = prop ? prop->GetValuePtr<std::string>() : nullptr;
	return val ? *val : "";
}
udm::Version udm::AssetData::GetAssetVersion() const
{
	auto prop = (*this)["assetVersion"];
	auto *version = prop ? prop->GetValuePtr<Version>() : nullptr;
	return version ? *version : Version{0};
}
void udm::AssetData::SetAssetType(const std::string &assetType) {(*this)[Data::KEY_ASSET_TYPE] = assetType;}
void udm::AssetData::SetAssetVersion(Version version) {(*this)[Data::KEY_ASSET_VERSION] = version;}
udm::LinkedPropertyWrapper udm::AssetData::GetData() const {return (*this)[Data::KEY_ASSET_DATA];}

std::string udm::Data::GetAssetType() const {return AssetData{*m_rootProperty}.GetAssetType();}
udm::Version udm::Data::GetAssetVersion() const {return AssetData{*m_rootProperty}.GetAssetVersion();}
void udm::Data::SetAssetType(const std::string &assetType) {return AssetData{*m_rootProperty}.SetAssetType(assetType);}
void udm::Data::SetAssetVersion(Version version) {return AssetData{*m_rootProperty}.SetAssetVersion(version);}

void udm::Data::ToAscii(std::stringstream &ss) const
{
	assert(m_rootProperty->type == Type::Element);
	if(m_rootProperty->type == Type::Element)
		static_cast<Element*>(m_rootProperty->value)->ToAscii(ss);
}

void udm::Element::ToAscii(std::stringstream &ss,const std::optional<std::string> &prefix) const
{
	auto childPrefix = prefix.has_value() ? (*prefix +'\t') : "";
	auto first = true;
	for(auto &pair : children)
	{
		if(first)
			first = false;
		else
			ss<<"\n";
		pair.second->ToAscii(ss,pair.first,childPrefix);
	}
}

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
	ss<<prefix<<"$"<<enum_type_to_ascii(type)<<" "<<propName<<" ";
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

bool udm::Data::Save(VFilePtrReal &f,std::string &outErr) const
{
	f->Write<Header>(m_header);
	WriteProperty(f,*m_rootProperty);
	return true;
}

bool udm::Data::SaveAscii(const std::string &fileName,std::string &outErr) const
{
	auto f = FileManager::OpenFile<VFilePtrReal>(fileName.c_str(),"w");
	if(f == nullptr)
	{
		outErr = "Unable to open file!";
		return false;
	}
	return SaveAscii(f,outErr);
}
bool udm::Data::SaveAscii(VFilePtrReal &f,std::string &outErr) const
{
	std::stringstream ss;
	ToAscii(ss);
	f->WriteString(ss.str());
	return true;
}

udm::LinkedPropertyWrapper udm::Data::operator[](const std::string &key) const {return LinkedPropertyWrapper{*m_rootProperty}[KEY_ASSET_DATA][key];}
udm::Element *udm::Data::operator->() {return &operator*();}
const udm::Element *udm::Data::operator->() const {return const_cast<Data*>(this)->operator->();}
udm::Element &udm::Data::operator*() {return GetAssetData()->GetValue<Element>();}
const udm::Element &udm::Data::operator*() const {return const_cast<Data*>(this)->operator*();}
udm::AssetData udm::Data::GetAssetData() const {return AssetData{*m_rootProperty};}
#pragma optimize("",on)
