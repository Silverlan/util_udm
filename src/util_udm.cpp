/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/base64.hpp>
#include <sstream>

#pragma optimize("",off)
namespace udm
{
	static std::string CONTROL_CHARACTERS = "{}[]$,";
	bool is_whitespace_character(char c) {return ustring::WHITESPACE.find(c) != std::string::npos;}
	bool is_control_character(char c) {return CONTROL_CHARACTERS.find(c) != std::string::npos;}

	class AsciiReader
	{
	public:
		static std::shared_ptr<udm::Data> LoadAscii(const VFilePtr &f);
	private:
		enum class BlockResult : uint8_t
		{
			EndOfBlock = 0,
			EndOfFile
		};
		template<class TException>
			TException BuildException(const std::string &msg)
			{return TException{msg,m_curLine,(m_curCharPos > 0) ? (m_curCharPos -1) : 0};}
		char PeekNextChar();
		char ReadUntil(char c,std::string *optOut=nullptr);
		char ReadNextToken();
		void MoveCursorForward(uint32_t n);
		void SeekNextToken(const std::optional<char> &tSeek={});
		std::string ReadString();
		std::string ReadString(char initialC);
		void ReadValue(Type type,void *outData);
		void ReadValueList(Type type,const std::function<bool()> &valueHandler);
		template<typename T,typename TBase>
			void ReadTypedValueList(T &outData);
		template<typename T>
			void ReadFloatValueList(T &outData);
		void ReadBlobData(std::vector<uint8_t> &outData);
		BlockResult ReadBlockKeyValues(Element &parent);
		char ReadChar();

		VFilePtr m_file;
		uint32_t m_curCharPos = 0; // Current character relative to current line
		uint32_t m_curLine = 0;
	};
};

udm::AsciiException::AsciiException(const std::string &msg,uint32_t lineIdx,uint32_t charIdx)
	: Exception{msg +" in line " +std::to_string(lineIdx +1) +" (column " +std::to_string(charIdx +1) +")"},
	lineIndex{lineIdx},charIndex{charIdx}
{}

udm::Type udm::ascii_type_to_enum(const std::string &type)
{
	// Note: These have to match enum_type_to_ascii
	static std::unordered_map<std::string,Type> namedTypeToEnum = {
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
	return (it != namedTypeToEnum.end()) ? it->second : Type::Invalid;
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

std::optional<udm::FormatType> udm::Data::GetFormatType(const std::string &fileName,std::string &outErr)
{
	auto f = FileManager::OpenFile(fileName.c_str(),"rb");
	if(f == nullptr)
	{
		outErr = "Unable to open file!";
		return {};
	}
	return GetFormatType(f,outErr);
}
std::optional<udm::FormatType> udm::Data::GetFormatType(const VFilePtr &f,std::string &outErr)
{
	auto offset = f->Tell();
	try
	{
		auto udmData = Open(f);
		return udmData ? FormatType::Binary : FormatType::Ascii;
	}
	catch(const Exception &e)
	{
		outErr = e.what();
	}
	return {};
}

std::shared_ptr<udm::Data> udm::Data::Load(const std::string &fileName)
{
	auto f = FileManager::OpenFile(fileName.c_str(),"rb");
	if(f == nullptr)
	{
		throw FileError{"Unable to open file!"};
		return nullptr;
	}
	return Load(f);
}

bool udm::Data::Save(const std::string &fileName) const
{
	auto f = FileManager::OpenFile<VFilePtrReal>(fileName.c_str(),"wb");
	if(f == nullptr)
	{
		throw FileError{"Unable to open file!"};
		return nullptr;
	}
	return Save(f);
}

void udm::Data::WriteProperty(VFilePtrReal &f,const Property &o) {o.Write(f);}

udm::PProperty udm::Data::ReadProperty(const VFilePtr &f)
{
	auto prop = Property::Create();
	if(prop->Read(f) == false)
		return nullptr;
	return prop;
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

std::shared_ptr<udm::Data> udm::Data::Create(const std::string &assetType,Version assetVersion)
{
	auto udmData = std::shared_ptr<udm::Data>{new udm::Data{}};
	udmData->m_rootProperty = Property::Create<Element>();
	udmData->SetAssetType(assetType);
	udmData->SetAssetVersion(assetVersion);
	udmData->m_rootProperty->GetValue<Element>().Add(KEY_ASSET_DATA);
	return udmData;
}

std::shared_ptr<udm::Data> udm::Data::Create()
{
	return Create("",0);
}

std::shared_ptr<udm::Data> udm::Data::Open(const std::string &fileName)
{
	auto f = FileManager::OpenFile(fileName.c_str(),"rb");
	if(f == nullptr)
	{
		throw FileError{"Unable to open file!"};
		return nullptr;
	}
	return Open(f);
}
std::shared_ptr<udm::Data> udm::Data::Open(const VFilePtr &f)
{
	auto udmData = std::shared_ptr<udm::Data>{new udm::Data{}};
	udmData->m_header = f->Read<Header>();
	if(ustring::compare(udmData->m_header.identifier.data(),HEADER_IDENTIFIER,true,strlen(HEADER_IDENTIFIER)) == false)
	{
		throw InvalidFormatError{"Unexpected header identifier, file is not a valid UDM file!"};
		return nullptr;
	}
	if(udmData->m_header.version == 0)
	{
		throw InvalidFormatError{"Unexpected header version, file is not a valid UDM file!"};
		return nullptr;
	}
	if(udmData->m_header.version > VERSION)
	{
		throw InvalidFormatError{"File uses a newer UDM version (" +std::to_string(udmData->m_header.version) +") than is supported by this version of UDM (" +std::to_string(VERSION) +")!"};
		return nullptr;
	}
	udmData->m_file = f;
	return udmData;
}

bool udm::Data::ValidateHeaderProperties()
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
				throw InvalidFormatError{"Excepted type " +std::string{magic_enum::enum_name(requiredKeyTypes[i])} +" for KeyValue '" +std::string{requiredKeys[i]} +"', but got type " +std::string{magic_enum::enum_name(it->second->type)} +"!"};
				return false;
			}
			continue;
		}
		throw InvalidFormatError{"KeyValue '" +std::string{requiredKeys[i]} +"' not found! Not a valid UDM file!"};
		return false;
	}
	return true;
}

std::shared_ptr<udm::Data> udm::Data::Load(const VFilePtr &f)
{
	auto offset = f->Tell();
	std::shared_ptr<udm::Data> udmData = nullptr;
	try
	{
		udmData = Open(f);
	}
	catch(const Exception &e)
	{
		// Attempt to load ascii format
		f->Seek(offset);
		return AsciiReader::LoadAscii(f);
	}
	auto o = ReadProperty(f);
	if(o == nullptr)
	{
		throw InvalidFormatError{"Root element is invalid!"};
		return nullptr;
	}
	if(o->type != Type::Element)
	{
		throw InvalidFormatError{"Expected root element to be type Element, but is type " +std::string{magic_enum::enum_name(o->type)} +"!"};
		return nullptr;
	}
	udmData->m_rootProperty = o;
	udmData->m_file = nullptr; // Don't need the file handle anymore
	return udmData->ValidateHeaderProperties() ? udmData : nullptr;
}

udm::PProperty udm::Data::LoadProperty(const std::string_view &path) const
{
	auto f = m_file;
	if(f == nullptr)
	{
		throw FileError{"Invalid file handle!"};
		return nullptr;
	}
	f->Seek(sizeof(m_header));
	auto type = f->Read<Type>();
	return LoadProperty(type,std::string{KEY_ASSET_DATA} +"." +std::string{path});
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

udm::PProperty udm::Data::LoadProperty(Type type,const std::string_view &path) const
{
	auto end = path.find('.');
	auto name = path.substr(0,end);
	if(name.empty())
	{
		throw PropertyLoadError{"Invalid property name!"};
		return nullptr;
	}

	auto f = m_file;
	if(f == nullptr)
	{
		throw FileError{"Invalid file handle!"};
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
				throw PropertyLoadError{"Attempted to retrieve non-integer index from array type, which is not allowed!"};
				return nullptr;
			}
			auto i = ustring::to_int(str);
			auto valueType = f->Read<Type>();
			auto n = f->Read<decltype(Array::size)>();
			if(i >= n || i < 0)
			{
				throw PropertyLoadError{"Array index out of bounds!"};
				return nullptr;
			}
			
			if(is_trivial_type(valueType))
			{
				if(!isLast)
				{
					throw PropertyLoadError{"Non-trailing property '" +std::string{name} +"[" +str +"]' is of type " +std::string{magic_enum::enum_name(valueType)} +", but " +std::string{magic_enum::enum_name(Type::Element)} +" expected!"};
					return nullptr;
				}
				f->Seek(f->Tell() +i *size_of(valueType));
				auto prop = Property::Create();
				if(prop->Read(valueType,f) == false)
					return nullptr;
				return prop;
			}

			f->Seek(f->Tell() +sizeof(uint64_t));
			for(auto j=decltype(i){0u};j<i;++j)
				SkipProperty(f,valueType);

			if(isLast)
			{
				auto prop = Property::Create();
				if(prop->Read(valueType,f) == false)
					return nullptr;
				return prop;
			}
			return LoadProperty(valueType,path.substr(end +1));
		}
		throw PropertyLoadError{"Non-trailing property '" +std::string{name} +"' is of type " +std::string{magic_enum::enum_name(type)} +", but " +std::string{magic_enum::enum_name(Type::Element)} +" expected!"};
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
		throw PropertyLoadError{"Element with specified name not found!"};
		return nullptr;
	}
	// Skip all children until we get the the one we want
	for(auto i=decltype(ichild){0u};i<ichild;++i)
		SkipProperty(f,f->Read<Type>());

	if(isLast)
		return ReadProperty(f); // Read this property in full
	auto childType = f->Read<Type>();
	return LoadProperty(childType,path.substr(end +1));
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

bool udm::Element::operator==(const Element &other) const
{
	for(auto &pair : children)
	{
		auto it = other.children.find(pair.first);
		if(it == other.children.end())
			return false;
		if(*pair.second != *it->second)
			return false;
	}
	return true;
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

bool udm::Data::Save(VFilePtrReal &f) const
{
	f->Write<Header>(m_header);
	WriteProperty(f,*m_rootProperty);
	return true;
}

std::shared_ptr<udm::Data> udm::AsciiReader::LoadAscii(const VFilePtr &f)
{
	auto udmData = std::shared_ptr<udm::Data>{new udm::Data{}};
	auto rootProp = Property::Create<Element>();
	AsciiReader reader {};
	reader.m_file = f;
	auto res = reader.ReadBlockKeyValues(rootProp->GetValue<Element>());
	if(res != BlockResult::EndOfFile)
		throw reader.BuildException<SyntaxError>("Block has been terminated improperly");
	
	auto assetData = udmData->GetAssetData();
	auto udmAssetData = (*rootProp)["assetData"];
	if(!udmAssetData)
	{
		auto assetDataProp = rootProp;
		rootProp = Property::Create<Element>();
		rootProp->GetValue<Element>().AddChild("assetData",assetDataProp);
	}
	udmData->m_rootProperty = rootProp;
	return udmData->ValidateHeaderProperties() ? udmData : nullptr;
}

char udm::AsciiReader::ReadChar()
{
	auto c = m_file->ReadChar();
	if(c == EOF)
		return EOF;
	if(c == '\n')
	{
		++m_curLine;
		m_curCharPos = 0;
	}
	else
		++m_curCharPos;
	return c;
}

char udm::AsciiReader::PeekNextChar()
{
	auto &f = m_file;
	auto pos = f->Tell();
	auto c = f->ReadChar();
	f->Seek(pos);
	return c;
}

char udm::AsciiReader::ReadUntil(char c,std::string *optOut)
{
	auto &f = m_file;
	auto cur = ReadChar();
	for(;;)
	{
		if(cur == EOF || cur == c)
			return cur;
		if(optOut)
			*optOut += cur;
		cur = ReadChar();
	}
	if(optOut)
		*optOut += cur;
	return cur;
}

void udm::AsciiReader::SeekNextToken(const std::optional<char> &tSeek)
{
	auto t = ReadNextToken();
	if(t == EOF)
		return;
	if(tSeek.has_value() && t != *tSeek)
		return SeekNextToken(tSeek);
	assert(m_curCharPos > 0);
	--m_curCharPos;
	m_file->Seek(m_file->Tell() -1);
}

void udm::AsciiReader::MoveCursorForward(uint32_t n)
{
	for(auto i=decltype(n){0u};i<n;++i)
		ReadChar();
}

char udm::AsciiReader::ReadNextToken()
{
	auto &f = m_file;
	int c;
	for(;;)
	{
		c = ReadChar();
		if(c == EOF)
			return EOF;
		if(ustring::WHITESPACE.find(c) != std::string::npos)
			continue;
		if(c == '/')
		{
			auto cNext = PeekNextChar();
			if(cNext == '/')
			{
				f->Seek(f->Tell() +1);
				ReadUntil('\n');
				continue;
			}
			else if(cNext == '*')
			{
				f->Seek(f->Tell() +1);
				for(;;)
				{
					if(ReadUntil('*') == EOF)
						return EOF;
					if(PeekNextChar() == '/')
					{
						f->Seek(f->Tell() +1);
						break;
					}
				}
				continue;
			}
		}
		return c;
	}
	return EOF;
}

std::string udm::AsciiReader::ReadString(char initialC)
{
	auto &f = m_file;
	std::string str;
	auto t = initialC;
	if(t == EOF)
		return str;
	if(is_control_character(initialC))
		throw BuildException<SyntaxError>("Expected string, got control character '" +std::string{t} +"'");
	if(t == '\"')
	{
		for(;;)
		{
			auto c = ReadUntil('\"',&str);
			if(c == EOF)
				throw BuildException<SyntaxError>("Expected quotation mark to end string, got EOF");
			if(str.size() <= 1 || str.back() != '\\')
				return str;
			continue;
		}
		// Unreachable
		return str;
	}

	f->Seek(f->Tell() -1);
	--m_curCharPos;
	for(;;)
	{
		t = PeekNextChar();
		if(t == EOF)
			return str;
		if(is_whitespace_character(t) || is_control_character(t))
			return str;
		str += ReadChar();
	}
	return str;
}

std::string udm::AsciiReader::ReadString()
{
	auto &f = m_file;
	return ReadString(f->ReadChar());
}

template<typename T>
	static T to_int(const std::string &str) {return static_cast<T>(util::to_int(str));}

template<typename T,typename TBase>
	void udm::AsciiReader::ReadTypedValueList(T &outData)
{
	static_assert((sizeof(T) %sizeof(TBase)) == 0);
	constexpr auto numExpectedValues = sizeof(T) /sizeof(TBase);
	std::array<float,numExpectedValues> values;
	uint32_t idx = 0;
	constexpr auto type = udm::type_to_enum<TBase>();
	ReadValueList(type,[this,&type,&values,&idx]() -> bool {
		if(idx >= values.size())
			return false;
		ReadValue(type,&values[idx++]);
		return true;
	});
	if(idx != values.size())
		throw BuildException<SyntaxError>("Expected " +std::to_string(values.size()) +" values for property definition, got " +std::to_string(idx) +" at");
	memcpy(&outData,values.data(),sizeof(outData));
}

template<typename T>
	void udm::AsciiReader::ReadFloatValueList(T &outData)
{
	ReadTypedValueList<T,float>(outData);
}

void udm::AsciiReader::ReadValueList(Type type,const std::function<bool()> &valueHandler)
{
	auto t = ReadNextToken();
	if(t != '[')
		throw BuildException<SyntaxError>("Expected '[' to initiate value list, got '" +std::string{t} +"'");
	uint32_t depth = 1;
	for(;;)
	{
		t = ReadNextToken();
		if(t == '[')
		{
			++depth;
			continue;
		}
		else if(t == ']')
		{
			assert(depth > 0);
			if(--depth == 0)
				return;
			continue;
		}
		else if(t == EOF)
			throw BuildException<SyntaxError>("Unexpected EOF");
		if(t != ',')
		{
			assert(m_curCharPos > 0);
			--m_curCharPos;
			m_file->Seek(m_file->Tell() -1);
		}
		if(valueHandler() == false)
			throw BuildException<DataError>("Invalid value for type '" +std::string{enum_type_to_ascii(type)} +"'");
	}
}

void udm::AsciiReader::ReadBlobData(std::vector<uint8_t> &outData)
{
	auto t = ReadNextToken();
	if(t != '[')
		throw BuildException<SyntaxError>("Expected '[' to initiate blob data, got '" +std::string{t} +"'");
	auto start = m_file->Tell();
	ReadUntil(']');
	auto end = m_file->Tell();
	auto blobSize = end -start -1;
	std::string strData;
	m_file->Seek(start);
	strData.resize(blobSize);
	auto numRead = m_file->Read(strData.data(),blobSize);
	if(numRead != blobSize)
		throw BuildException<SyntaxError>("Unexpected end of blob data");
	// TODO: Optimize this so no copy is required!
	try
	{
		auto decoded = util::base64_decode(strData);
		outData.resize(decoded.size());
		memcpy(outData.data(),decoded.data(),outData.size());
	}
	catch(const std::runtime_error &e)
	{
		throw BuildException<DataError>(e.what());
	}
	MoveCursorForward(1); // Move past ']'
}

constexpr bool is_float_based_type(udm::Type type)
{
	switch(type)
	{
	case udm::Type::Vector2:
	case udm::Type::Vector3:
	case udm::Type::Vector4:
	case udm::Type::Quaternion:
	case udm::Type::EulerAngles:
	case udm::Type::Transform:
	case udm::Type::ScaledTransform:
	case udm::Type::Mat4:
	case udm::Type::Mat3x4:
		return true;
	}
	return false;
}

void udm::AsciiReader::ReadValue(Type type,void *outData)
{
	if(is_float_based_type(type))
	{
		std::visit([this,&outData](auto tag) {
			using T = decltype(tag)::type;
			auto &v = *static_cast<T*>(outData);
			if constexpr(is_float_based_type(type_to_enum_s<T>())) // Always true in this context!
				ReadFloatValueList(v);
			// Note: Quaternions are stored in the Ascii format in w-x-y-z order, but their binary data
			// in glm is stored as x-y-z-w, so we have to re-order the components here.
			auto fixQuatComponentOrder = [](Quat &v) {
				v = {v.x,v.y,v.z,v.w};
			};
			if constexpr(std::is_same_v<T,Quaternion>)
				fixQuatComponentOrder(v);
			else if constexpr(std::is_same_v<T,Transform> || std::is_same_v<T,ScaledTransform>)
				fixQuatComponentOrder(v.GetRotation());
		},get_generic_tag(type));
		return;
	}
	switch(type)
	{
	case Type::Nil:
		break;
	case Type::String:
	{
		*static_cast<String*>(outData) = ReadString();
		break;
	}
	case Type::Utf8String:
	{
		auto &str = *static_cast<Utf8String*>(outData);
		ReadValueList(type,[this]() -> bool {
			std::string str;
			ReadValue(udm::Type::String,&str);
			return str == "base64";
		});

		ReadBlobData(str.data);
		break;
	}
	case Type::Int8:
		*static_cast<Int8*>(outData) = to_int<Int8>(ReadString());
		break;
	case Type::UInt8:
		*static_cast<UInt8*>(outData) = to_int<UInt8>(ReadString());
		break;
	case Type::Int16:
		*static_cast<Int16*>(outData) = to_int<Int16>(ReadString());
		break;
	case Type::UInt16:
		*static_cast<UInt16*>(outData) = to_int<UInt16>(ReadString());
		break;
	case Type::Int32:
		*static_cast<Int32*>(outData) = to_int<Int32>(ReadString());
		break;
	case Type::UInt32:
		*static_cast<UInt32*>(outData) = static_cast<UInt32>(util::to_uint64(ReadString()));
		break;
	case Type::Int64:
		*static_cast<Int64*>(outData) = static_cast<Int64>(atoll(ReadString().c_str()));
		break;
	case Type::UInt64:
		*static_cast<UInt64*>(outData) = static_cast<UInt64>(util::to_uint64(ReadString()));
		break;
	case Type::Float:
		*static_cast<Float*>(outData) = util::to_float(ReadString());
		break;
	case Type::Double:
		*static_cast<Double*>(outData) = static_cast<Double>(stod(ReadString()));
		break;
	case Type::Boolean:
		*static_cast<Boolean*>(outData) = util::to_boolean(ReadString());
		break;
	case Type::Srgba:
		ReadTypedValueList<Srgba,Srgba::value_type>(*static_cast<Srgba*>(outData));
		break;
	case Type::HdrColor:
		ReadTypedValueList<HdrColor,HdrColor::value_type>(*static_cast<HdrColor*>(outData));
		break;
	case Type::Blob:
	{
		auto &blob = *static_cast<Blob*>(outData);
		ReadBlobData(blob.data);
		break;
	}
	case Type::BlobLz4:
	{
		auto &blob = *static_cast<BlobLz4*>(outData);
		blob.uncompressedSize = 0;
		ReadValueList(type,[this,&blob]() -> bool {
			static_assert(sizeof(blob.uncompressedSize) == size_of(Type::UInt64));
			ReadValue(udm::Type::UInt64,&blob.uncompressedSize);
			return true;
		});
		
		ReadBlobData(blob.compressedData);
		break;
	}
	case Type::Element:
	{
		auto t = ReadNextToken();
		if(t != '{')
			throw BuildException<SyntaxError>("Expected '{' for array element block definition, got '" +std::string{t} +"'");
		if(ReadBlockKeyValues(*static_cast<Element*>(outData)) == BlockResult::EndOfFile)
			throw BuildException<SyntaxError>("Unexpected end of file");
		break;
	}
	case Type::Array:
	{
		auto valueType = Type::Invalid;
		ReadValueList(type,[this,&valueType]() -> bool {
			auto valueTypeStr = ReadString();
			valueType = ascii_type_to_enum(valueTypeStr);
			return valueType != Type::Invalid;
		});
		if(valueType == Type::Invalid)
			throw BuildException<SyntaxError>("Missing values for array");

		auto &a = *static_cast<Array*>(outData);
		a.SetValueType(valueType);
		uint32_t numValues = 0;
		a.Resize(100);
		ReadValueList(valueType,[this,valueType,&a,&numValues]() -> bool {
			if(numValues >= a.GetSize())
				a.Resize(numValues *2 +20);
			ReadValue(valueType,a.GetValuePtr(numValues));
			++numValues;
			return true;
		});
		a.Resize(numValues);
		break;
	}
	};
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
}

udm::AsciiReader::BlockResult udm::AsciiReader::ReadBlockKeyValues(Element &parent)
{
	auto &f = m_file;
	for(;;)
	{
		auto t = ReadNextToken();
		if(t == '$')
		{
			// Variable
			auto type = ReadString();
			auto eType = ascii_type_to_enum(type.c_str());
			if(eType == udm::Type::Invalid)
				throw BuildException<SyntaxError>("Invalid keyvalue type '" +type +"' found");
			auto key = ReadString(ReadNextToken());
			SeekNextToken();
			auto prop = Property::Create(eType);
			ReadValue(eType,prop->value);
			parent.AddChild(key,prop);
			continue;
		}
		if(t == '}')
			return BlockResult::EndOfBlock;
		if(t == EOF)
			return BlockResult::EndOfFile;
		if(is_control_character(t))
			throw BuildException<SyntaxError>("Expected variable or child block, got unexpected control character '" +std::string{t} +"'");
		// Name of a new block
		auto childBlockName = ReadString(t);
		t = ReadNextToken();
		if(t != '{')
			throw BuildException<SyntaxError>("Expected '{' for child block definition, got '" +std::string{t} +"'");
		
		auto child = Property::Create<Element>();
		if(ReadBlockKeyValues(child->GetValue<Element>()) == BlockResult::EndOfFile)
			throw BuildException<SyntaxError>("Unexpected end of file");
		parent.AddChild(childBlockName,child);
	}
	// Unreachable
	return BlockResult::EndOfFile;
}

bool udm::Data::SaveAscii(const std::string &fileName) const
{
	auto f = FileManager::OpenFile<VFilePtrReal>(fileName.c_str(),"w");
	if(f == nullptr)
	{
		throw FileError{"Unable to open file!"};
		return false;
	}
	return SaveAscii(f);
}
bool udm::Data::SaveAscii(VFilePtrReal &f) const
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

bool udm::Data::operator==(const Data &other) const
{
	auto data0 = GetAssetData().GetData();
	auto data1 = other.GetAssetData().GetData();
	if(!data0 || !data1)
		return false;
	return *data0.prop == *data1.prop;
}
#pragma optimize("",on)
