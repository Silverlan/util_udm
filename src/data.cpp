/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/util_string.h>

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
std::optional<udm::FormatType> udm::Data::GetFormatType(const ::VFilePtr &f,std::string &outErr)
{
	return GetFormatType(std::make_unique<VFilePtr>(f),outErr);
}
std::optional<udm::FormatType> udm::Data::GetFormatType(std::unique_ptr<IFile> &&f,std::string &outErr)
{
	auto offset = f->Tell();
	try
	{
		try
		{
			auto udmData = Open(std::move(f));
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
		return false;
	}
	VFilePtr fp{f};
	return Save(fp);
}

void udm::Data::WriteProperty(IFile &f,const Property &o) {o.Write(f);}

udm::PProperty udm::Data::ReadProperty(IFile &f)
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

bool udm::Data::DebugTest()
{
	try
	{
		auto data = udm::Data::Create();
		auto udmData = data->GetAssetData().GetData();
		auto el = udmData["first"]["second"]["third"];
		el["stringTest"] = "Test";
		el["floatTest"] = 5.f;
		el["path/test/value"] = Vector3{1,2,3};

		auto udmCompressedElementArray = el.AddArray("compressedElementArray",1,Type::Element,ArrayType::Compressed);
		auto udmEl = udmCompressedElementArray[0];
		udmEl["test"] = "555";

		auto testCmp = (udmEl["test"] == "555");
		if(!testCmp)
			throw Exception {"Internal library error: UDM property comparison failure"};

		el["NIL"] = udm::Nil{};
		el["STRING"] = udm::String{"Hello"};
		el["UTF8STRING"] = udm::Utf8String{};
		el["INT8"] = udm::Int8{6};
		el["UINT8"] = udm::UInt8{5};
		el["INT16"] = udm::Int16{4};
		el["UINT16"] = udm::UInt16{1'234};
		el["INT32"] = udm::Int32{6'655};
		el["UINT32"] = udm::UInt32{3'645};
		el["INT64"] = udm::Int64{5'345'465};
		el["UINT64"] = udm::UInt64{5'343'242};
		el["FLOAT"] = udm::Float{123.5411};
		el["DOUBLE"] = udm::Double{864.546};
		el["BOOLEAN"] = udm::Boolean{true};

		el["VECTOR2"] = udm::Vector2{7,8};
		el["VECTOR3"] = udm::Vector3{4,3,5};
		el["VECTOR4"] = udm::Vector4{8,4,6,3};
		el["QUATERNION"] = udm::Quaternion{4,8,6,5};
		el["EULERANGLES"] = udm::EulerAngles{4,3,5};
		el["SRGBA"] = udm::Srgba{255,128,54,44};
		el["HDRCOLOR"] = udm::HdrColor{800,500,12};
		el["TRANSFORM"] = udm::Transform{Vector3{6,4,7},Quat{1,8,5,4}};
		el["SCALEDTRANSFORM"] = udm::ScaledTransform{Vector3{9,6,7},Quat{4,3,6,4},Vector3{4,6,3}};
		el["MAT4"] = umat::identity();
		el["MAT3X4"] = udm::Mat3x4{6,5,8,4,6,4,5,7,5,6,3,2};
	
		el["BLOB"] = udm::Blob{};
		el["BLOBLZ4"] = udm::BlobLz4{};
		el["REFERENCE"] = udm::Reference{"path/test/value"};

		auto a = el.AddArray("ARRAY",{},udm::Type::Int32);
		a.Resize(12);
		a[0] = 5;
		a[1] = 3;
		a[4] = 6;
		a[11] = 11;

		std::vector<float> testData;
		testData.resize(100);
		for(auto i=decltype(testData.size()){0u};i<testData.size();++i)
			testData[i] = i;

		std::vector<uint8_t> testDataBytes {};
		testDataBytes.resize(testData.size() *sizeof(testData[0]));
		el["float_blob"] = udm::Blob{std::move(testDataBytes)};
		el["float_blob_lz4"] = udm::compress_lz4_blob(testData);
		el["float_array"] = testData;

		el["halfTest"] = Half{0.573f};
		
		struct TestStruct
		{
			Float f;
			Vector3 v;
			Int8 i;
			Half h;
		};
		auto structDef = StructDescription::Define<float,Vector3,int8_t,Half,uint8_t>({"Float","Vector3","Int8","Half","Padding"});
		el["structTest"] = Struct{structDef};
		// TODO: Assign struct values?

		auto test = el["path/test"];
		auto elArray = test.AddArray("elements",5);
		elArray[3]["sub-element"]["1"] = Vector3{1,1,1};

		//udmData.AddArray("test",1,udm::Type::Float);
		//udmData["test"][0] = 5.f;
		auto aCompressed = udmData.AddArray("compressedArray",10,Type::Float,ArrayType::Compressed);
		for(auto i=0;i<10;++i)
			udmData["compressedArray"][i] = i;

		auto aCompressedString = udmData.AddArray("compressedStringArray",3,Type::String,ArrayType::Compressed);
		aCompressedString[0] = "Lorem Ipsum";
		aCompressedString[1] = "";
		aCompressedString[2] = "B";

		auto structArray = udmData.AddArray(
			"structArray",
			structDef,
			10
		);
		for(auto i=0;i<10;++i)
			structArray[i] = TestStruct{5.f,Vector3{3,3,3},2,0.34f};
		
		auto aCompressedStructArray = udmData.AddArray(
			"compressedStructArray",
			structDef,
			10,ArrayType::Compressed
		);
		for(auto i=0;i<10;++i)
			aCompressedStructArray[i] = TestStruct{5.f,Vector3{3,3,3},2,0.34f};

		std::vector<TestStruct> structuredData = {
			TestStruct{5.f,Vector3{3,3,3},2,0.34f}
		};
		auto aCompressedStructArray2 = udmData.AddArray(
			"compressedStructArray2",
			structDef,
			structuredData,ArrayType::Compressed
		);

		{
			auto aChanging = udmData.AddArray<Vector3>("trivialChangingArray",std::vector<Vector3>{Vector3{1,2,3},Vector3{4,5,6},Vector3{7,8,9},Vector3{10,11,12},Vector3{13,14,15}});
			std::vector<Vector3> values;
			aChanging(values);
			if(values != std::vector<Vector3>{Vector3{1,2,3},Vector3{4,5,6},Vector3{7,8,9},Vector3{10,11,12},Vector3{13,14,15}})
				throw Exception {"trivialChangingArray mismatch (1)"};

			aChanging.Resize(8);
			aChanging(values);
			if(values != std::vector<Vector3>{Vector3{1,2,3},Vector3{4,5,6},Vector3{7,8,9},Vector3{10,11,12},Vector3{13,14,15},Vector3{},Vector3{},Vector3{}})
				throw Exception {"trivialChangingArray mismatch (2)"};

			aChanging.GetValue<udm::Array>().RemoveValue(1);
			aChanging(values);
			if(values != std::vector<Vector3>{Vector3{1,2,3},Vector3{7,8,9},Vector3{10,11,12},Vector3{13,14,15},Vector3{},Vector3{},Vector3{}})
				throw Exception {"trivialChangingArray mismatch (3)"};

			aChanging.GetValue<udm::Array>().InsertValue(3,Vector3{5,5,5});
			aChanging(values);
			if(values != std::vector<Vector3>{Vector3{1,2,3},Vector3{7,8,9},Vector3{10,11,12},Vector3{5,5,5},Vector3{13,14,15},Vector3{},Vector3{},Vector3{}})
				throw Exception {"trivialChangingArray mismatch (4)"};

			aChanging.Resize(4);
			aChanging(values);
			if(values != std::vector<Vector3>{Vector3{1,2,3},Vector3{7,8,9},Vector3{10,11,12},Vector3{5,5,5}})
				throw Exception {"trivialChangingArray mismatch (5)"};
		}

		{
			auto aChanging = udmData.AddArray<String>("nonTrivialChangingArray",std::vector<String>{String{"1,2,3"},String{"4,5,6"},String{"7,8,9"},String{"10,11,12"},String{"13,14,15"}});
			std::vector<String> values;
			aChanging(values);
			if(values != std::vector<String>{String{"1,2,3"},String{"4,5,6"},String{"7,8,9"},String{"10,11,12"},String{"13,14,15"}})
				throw Exception {"nonTrivialChangingArray mismatch (1)"};

			aChanging.Resize(8);
			aChanging(values);
			if(values != std::vector<String>{String{"1,2,3"},String{"4,5,6"},String{"7,8,9"},String{"10,11,12"},String{"13,14,15"},String{""},String{""},String{""}})
				throw Exception {"nonTrivialChangingArray mismatch (2)"};

			aChanging.GetValue<udm::Array>().RemoveValue(1);
			aChanging(values);
			if(values != std::vector<String>{String{"1,2,3"},String{"7,8,9"},String{"10,11,12"},String{"13,14,15"},String{""},String{""},String{""}})
				throw Exception {"nonTrivialChangingArray mismatch (3)"};

			aChanging.GetValue<udm::Array>().InsertValue(3,String{"5,5,5"});
			aChanging(values);
			if(values != std::vector<String>{String{"1,2,3"},String{"7,8,9"},String{"10,11,12"},String{"5,5,5"},String{"13,14,15"},String{""},String{""},String{""}})
				throw Exception {"nonTrivialChangingArray mismatch (4)"};

			aChanging.Resize(4);
			aChanging(values);
			if(values != std::vector<String>{String{"1,2,3"},String{"7,8,9"},String{"10,11,12"},String{"5,5,5"}})
				throw Exception {"nonTrivialChangingArray mismatch (5)"};
		}

		if(*data != *data)
			throw Exception {"Internal library error: UDM data does not match itself?"};

		//auto aNested = el["path"]["nestedArrays"][0][0];
		//aNested["a"] = 5.f;
		//aNested["b"] = 3.f;

		auto fTestFileIo = [&data](const std::string &fileName,bool binary) {
			auto fw = FileManager::OpenFile<VFilePtrReal>(fileName.c_str(),binary ? "wb" : "w");
			if(fw)
			{
				if(binary)
				{
					VFilePtr fp{fw};
					data->Save(fp);
				}
				else
				{
					VFilePtr fp{fw};
					data->SaveAscii(fp);
				}
				fw = nullptr;
			}
			else
				throw Exception {"Unable to write '" +fileName +"'"};

			auto fr = FileManager::OpenFile(fileName.c_str(),binary ? "rb" : "r");
			if(fr)
			{
				auto udmDataLoad = udm::Data::Load(fr);
				if(udmDataLoad == nullptr)
					throw Exception {"Failed to load '" +fileName +"'"};
				auto same = (*data == *udmDataLoad);
				if(!same)
					throw Exception {"Mismatch between written data and loaded data!"};

				auto udmCompressedArray = udmDataLoad->GetAssetData().GetData()["compressedArray"];
				int val = -1;
				udmCompressedArray[3](val);
				if(val != 3)
					throw Exception {"Incorrect compressed array value!"};

				std::vector<float> arrayData;
				udmCompressedArray.GetBlobData(arrayData);
				if(arrayData != std::vector<float>{0,1,2,3,4,5,6,7,8,9})
					throw Exception {"Incorrect compressed array value!"};

			}
			else
				throw Exception {"Unable to load '" +fileName +"'"};
		};
		fTestFileIo("udm_test.udm",false);
		fTestFileIo("udm_test.udm_b",true);
	}
	catch(const Exception &e)
	{
		std::cout<<"UDM debug test failed: "<<e.what()<<std::endl;
		return false;
	}
	return true;
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
std::shared_ptr<udm::Data> udm::Data::Open(const ::VFilePtr &f) {return Open(std::make_unique<VFilePtr>(f));}
std::shared_ptr<udm::Data> udm::Data::Open(std::unique_ptr<IFile> &&f)
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
	udmData->m_file = std::move(f);
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
	std::shared_ptr<udm::Data> load_ascii(std::unique_ptr<IFile> &&f);
};
std::shared_ptr<udm::Data> udm::Data::Load(const ::VFilePtr &f) {return Load(std::make_unique<VFilePtr>(f));}
std::shared_ptr<udm::Data> udm::Data::Load(std::unique_ptr<IFile> &&f)
{
	auto offset = f->Tell();
	std::shared_ptr<udm::Data> udmData = nullptr;
	try
	{
		udmData = Open(std::move(f));
	}
	catch(const Exception &e)
	{
		// Attempt to load ascii format
		f->Seek(offset);
		return load_ascii(std::move(f));
	}
	auto o = ReadProperty(*udmData->m_file);
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

void udm::Data::ResolveReferences()
{
	auto root = GetAssetData().GetData();
	std::function<void(LinkedPropertyWrapper&)> resolveReferences = nullptr;
	resolveReferences = [&resolveReferences,&root](LinkedPropertyWrapper &prop) {
		if(!prop)
			return;
		if(prop.IsType(Type::Element))
		{
			for(auto pair : prop.ElIt())
				resolveReferences(pair.property);
		}
		else if(prop.IsType(Type::Array))
		{
			if(prop.GetValue<Array>().IsValueType(Type::Element))
			{
				for(auto child : prop)
					resolveReferences(child);
			}
		}
		else if(prop.IsType(Type::Reference))
		{
			auto &ref = prop.GetValue<Reference>();
			ref.InitializeProperty(root);
		}
	};
	resolveReferences(root);
}

udm::PProperty udm::Data::LoadProperty(const std::string_view &path) const
{
	if(m_file == nullptr)
	{
		throw FileError{"Invalid file handle!"};
		return nullptr;
	}
	auto &f = *m_file;
	f.Seek(sizeof(m_header));
	auto type = f.Read<Type>();
	return LoadProperty(type,std::string{KEY_ASSET_DATA} +"." +std::string{path});
}

void udm::Data::SkipProperty(IFile &f,Type type)
{
	if(is_numeric_type(type) || is_generic_type(type))
	{
		f.Seek(f.Tell() +size_of(type));
		return;
	}
	switch(type)
	{
	case Type::String:
	case Type::Reference:
	{
		uint32_t len = f.Read<uint8_t>();
		if(len == Property::EXTENDED_STRING_IDENTIFIER)
			len = f.Read<uint32_t>();
		f.Seek(f.Tell() +len);
		break;
	}
	case Type::Utf8String:
	{
		auto size = f.Read<uint32_t>();
		f.Seek(f.Tell() +size);
		break;
	}
	case Type::Blob:
	{
		auto size = f.Read<size_t>();
		f.Seek(f.Tell() +size);
		break;
	}
	case Type::BlobLz4:
	{
		auto compressedSize = f.Read<size_t>();
		f.Seek(f.Tell() +sizeof(size_t) +compressedSize);
		break;
	}
	case Type::Array:
	{
		using TSize = decltype(std::declval<Array>().GetSize());
		auto valueType = f.Read<Type>();
		if(is_non_trivial_type(valueType))
		{
			f.Seek(f.Tell() +sizeof(TSize));
			auto sizeBytes = f.Read<uint64_t>();
			f.Seek(f.Tell() +sizeBytes);
			break;
		}
		auto size = f.Read<TSize>();
		f.Seek(f.Tell() +size *size_of(valueType));
		break;
	}
	case Type::ArrayLz4:
	{
		auto compressedSize = f.Read<size_t>();
		auto valueType = f.Read<Type>();
		
		if(valueType == Type::Struct)
		{
			auto offsetToEndOfStructuredDataHeader = f.Read<StructDescription::SizeType>();
			f.Seek(f.Tell() +offsetToEndOfStructuredDataHeader);
		}
		else if(valueType == Type::Element)
			f.Seek(f.Tell() +sizeof(size_t));

		using TSize = decltype(std::declval<Array>().GetSize());
		f.Seek(f.Tell() +sizeof(TSize) +compressedSize);
		break;
	}
	case Type::Element:
	{
		auto size = f.Read<uint64_t>();
		f.Seek(f.Tell() +size);
		break;
	}
	case Type::Struct:
	{
		auto size = f.Read<StructDescription::SizeType>();
		f.Seek(f.Tell() +size);
		break;
	}
	}
	static_assert(NON_TRIVIAL_TYPES.size() == 9);
}

std::string udm::Data::ReadKey(IFile &f)
{
	auto len = f.Read<uint8_t>();
	std::string str;
	str.resize(len);
	f.Read(str.data(),len);
	return str;
}
void udm::Data::WriteKey(IFile &f,const std::string &key)
{
	if(key.length() > std::numeric_limits<uint8_t>::max())
		return WriteKey(f,key.substr(0,std::numeric_limits<uint8_t>::max()));
	f.Write<uint8_t>(key.length());
	f.Write(key.data(),key.length());
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

	if(m_file == nullptr)
	{
		throw FileError{"Invalid file handle!"};
		return nullptr;
	}
	auto &f = *m_file;
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
			auto valueType = f.Read<Type>();
			using TSize = decltype(std::declval<Array>().GetSize());
			auto n = f.Read<TSize>();
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
				f.Seek(f.Tell() +i *size_of(valueType));
				auto prop = Property::Create();
				if(prop->Read(valueType,f) == false)
					return nullptr;
				return prop;
			}

			f.Seek(f.Tell() +sizeof(uint64_t));
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

	auto elStartOffset = f.Tell();
	f.Seek(f.Tell() +sizeof(uint64_t));
	auto numChildren = f.Read<uint32_t>();
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
		SkipProperty(f,f.Read<Type>());

	if(isLast)
		return ReadProperty(f); // Read this property in full
	auto childType = f.Read<Type>();
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
void udm::AssetData::SetAssetType(const std::string &assetType) const {(*this)[Data::KEY_ASSET_TYPE] = assetType;}
void udm::AssetData::SetAssetVersion(Version version) const {(*this)[Data::KEY_ASSET_VERSION] = version;}
udm::LinkedPropertyWrapper udm::AssetData::GetData() const {return (*this)[Data::KEY_ASSET_DATA];}

std::string udm::Data::GetAssetType() const {return AssetData{*m_rootProperty}.GetAssetType();}
udm::Version udm::Data::GetAssetVersion() const {return AssetData{*m_rootProperty}.GetAssetVersion();}
void udm::Data::SetAssetType(const std::string &assetType) {return AssetData{*m_rootProperty}.SetAssetType(assetType);}
void udm::Data::SetAssetVersion(Version version) {return AssetData{*m_rootProperty}.SetAssetVersion(version);}

void udm::Data::ToAscii(std::stringstream &ss,AsciiSaveFlags flags) const
{
	assert(m_rootProperty->type == Type::Element);
	if(m_rootProperty->type == Type::Element)
	{
		if(!umath::is_flag_set(flags,AsciiSaveFlags::IncludeHeader))
		{
			auto &elRoot = *static_cast<Element*>(m_rootProperty->value);
			auto udmAssetData = elRoot[Data::KEY_ASSET_DATA];
			if(udmAssetData && udmAssetData->IsType(Type::Element))
				udmAssetData->GetValue<Element>().ToAscii(flags,ss);
		}
		else
			static_cast<Element*>(m_rootProperty->value)->ToAscii(flags,ss);
	}
}

bool udm::Data::Save(IFile &f) const
{
	f.Write<Header>(m_header);
	WriteProperty(f,*m_rootProperty);
	return true;
}

bool udm::Data::Save(const ::VFilePtr &f)
{
	VFilePtr fp{f};
	return Save(fp);
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
	auto res = (*data0.prop == *data1.prop);
	UDM_ASSERT_COMPARISON(res);
	return res;
}

udm::VFilePtr::VFilePtr(const ::VFilePtr &f)
	: m_file{f}
{}
size_t udm::VFilePtr::Read(void *data,size_t size) {return m_file->Read(data,size);}
size_t udm::VFilePtr::Write(const void *data,size_t size)
{
	auto type = m_file->GetType();
	if(type != VFILE_LOCAL)
		return 0;
	return static_cast<VFilePtrInternalReal*>(m_file.get())->Write(data,size);
}
size_t udm::VFilePtr::Tell() {return m_file->Tell();}
void udm::VFilePtr::Seek(size_t offset,Whence whence)
{
	switch(whence)
	{
	case Whence::Cur:
		return m_file->Seek(offset,SEEK_CUR);
	case Whence::End:
		return m_file->Seek(offset,SEEK_END);
	}
	return m_file->Seek(offset,SEEK_SET);
}
int32_t udm::VFilePtr::ReadChar() {return m_file->ReadChar();}
