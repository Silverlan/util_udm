/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sharedutils/base64.hpp>
#include <sstream>

#pragma optimize("",off)
namespace udm
{
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

template<typename T>
	static T to_int(const std::string &str) {return static_cast<T>(util::to_int(str));}

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
		{"element",Type::Element},
		{"ref",Type::Reference}
	};
	static_assert(umath::to_integral(Type::Count) == 30,"Update this list when new types are added!");
	auto it = namedTypeToEnum.find(type);
	return (it != namedTypeToEnum.end()) ? it->second : Type::Invalid;
}

void udm::sanitize_key_name(std::string &key)
{
	ustring::replace(key,".","");
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
			if(type != Type::Array)
			{
				++depth;
				continue;
			}
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
		else
			SeekNextToken();
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
	case Type::Reference:
	{
		static_cast<Reference*>(outData)->path = ReadString();
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
		uint32_t size = 10;
		ReadValueList(type,[this,&valueType,&size]() -> bool {
			auto valueTypeStr = ReadString();
			auto sep = valueTypeStr.find(';');
			if(sep != std::string::npos)
			{
				size = util::to_uint(valueTypeStr.substr(sep +1));
				valueTypeStr = valueTypeStr.substr(0,sep);
			}
			valueType = ascii_type_to_enum(valueTypeStr);
			return valueType != Type::Invalid;
		});
		if(valueType == Type::Invalid)
			throw BuildException<SyntaxError>("Missing values for array");

		auto &a = *static_cast<Array*>(outData);
		a.SetValueType(valueType);
		uint32_t numValues = 0;
		a.Resize(size);
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
	static_assert(umath::to_integral(Type::Count) == 30,"Update this list when new types are added!");
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

bool udm::Data::SaveAscii(const std::string &fileName,bool includeHeader) const
{
	auto f = FileManager::OpenFile<VFilePtrReal>(fileName.c_str(),"w");
	if(f == nullptr)
	{
		throw FileError{"Unable to open file!"};
		return false;
	}
	return SaveAscii(f,includeHeader);
}
bool udm::Data::SaveAscii(VFilePtrReal &f,bool includeHeader) const
{
	std::stringstream ss;
	ToAscii(ss,includeHeader);
	f->WriteString(ss.str());
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
	auto udmAssetData = (*rootProp)[Data::KEY_ASSET_DATA];
	if(!udmAssetData)
	{
		auto assetDataProp = rootProp;
		rootProp = Property::Create<Element>();
		auto &elRoot = rootProp->GetValue<Element>();
		elRoot.AddChild(Data::KEY_ASSET_DATA,assetDataProp);
		elRoot[Data::KEY_ASSET_VERSION] = static_cast<uint32_t>(1);
		elRoot[Data::KEY_ASSET_TYPE] = "nil";
	}
	udmData->m_rootProperty = rootProp;
	return udmData->ValidateHeaderProperties() ? udmData : nullptr;
}

namespace udm
{
	std::shared_ptr<udm::Data> load_ascii(const VFilePtr &f)
	{
		return udm::AsciiReader::LoadAscii(f);
	}
};

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
#pragma optimize("",on)
