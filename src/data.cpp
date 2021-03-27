/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sharedutils/magic_enum.hpp>

#pragma optimize("",off)
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
		try
		{
			auto udmData = Open(f);
			return udmData ? FormatType::Binary : FormatType::Ascii;
		}
		catch(const Exception &e)
		{
			return FormatType::Ascii;
		}
		return FormatType::Ascii;
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

namespace udm
{
	std::shared_ptr<udm::Data> load_ascii(const VFilePtr &f);
};
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
		return load_ascii(f);
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
	case Type::Reference:
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
	static_assert(NON_TRIVIAL_TYPES.size() == 7);
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
	auto end = std::string::npos; // path.find('.');
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
	auto *val = prop ? prop.GetValuePtr<std::string>() : nullptr;
	return val ? *val : "";
}
udm::Version udm::AssetData::GetAssetVersion() const
{
	auto prop = (*this)["assetVersion"];
	auto *version = prop ? prop.GetValuePtr<Version>() : nullptr;
	return version ? *version : Version{0};
}
void udm::AssetData::SetAssetType(const std::string &assetType) {(*this)[Data::KEY_ASSET_TYPE] = assetType;}
void udm::AssetData::SetAssetVersion(Version version) {(*this)[Data::KEY_ASSET_VERSION] = version;}
udm::LinkedPropertyWrapper udm::AssetData::GetData() const {return (*this)[Data::KEY_ASSET_DATA];}

std::string udm::Data::GetAssetType() const {return AssetData{*m_rootProperty}.GetAssetType();}
udm::Version udm::Data::GetAssetVersion() const {return AssetData{*m_rootProperty}.GetAssetVersion();}
void udm::Data::SetAssetType(const std::string &assetType) {return AssetData{*m_rootProperty}.SetAssetType(assetType);}
void udm::Data::SetAssetVersion(Version version) {return AssetData{*m_rootProperty}.SetAssetVersion(version);}

void udm::Data::ToAscii(std::stringstream &ss,bool includeHeader) const
{
	assert(m_rootProperty->type == Type::Element);
	if(m_rootProperty->type == Type::Element)
	{
		if(!includeHeader)
		{
			auto &elRoot = *static_cast<Element*>(m_rootProperty->value);
			auto udmAssetData = elRoot[Data::KEY_ASSET_DATA];
			if(udmAssetData && udmAssetData->IsType(Type::Element))
				udmAssetData->GetValue<Element>().ToAscii(ss);
		}
		else
			static_cast<Element*>(m_rootProperty->value)->ToAscii(ss);
	}
}

bool udm::Data::Save(VFilePtrReal &f) const
{
	f->Write<Header>(m_header);
	WriteProperty(f,*m_rootProperty);
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
