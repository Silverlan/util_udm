/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_HPP__
#define __UDM_HPP__

#include "udm_definitions.hpp"
#include <array>
#include <cinttypes>
#include <vector>
#include <memory>
#include <string>
#include <cassert>
#include <optional>
#include <variant>
#include <map>
#include <sstream>
#include <unordered_map>
#include <mathutil/uvec.h>
#include <mathutil/transform.hpp>
#include <sharedutils/util.h>
#include <sharedutils/magic_enum.hpp>
#include <sharedutils/util_ifile.hpp>
#include <fsys/filesystem.h>
#include "udm_types.hpp"
#include "udm_trivial_types.hpp"
#include "udm_conversion.hpp"
#include "udm_exception.hpp"
#include "udm_enums.hpp"

#pragma warning( push )
#pragma warning( disable : 4715 )
namespace udm
{
	static std::string CONTROL_CHARACTERS = "{}[]<>$,:;";
	static std::string WHITESPACE_CHARACTERS = " \t\f\v\n\r";
	static constexpr auto PATH_SEPARATOR = '/';
	DLLUDM bool is_whitespace_character(char c);
	DLLUDM bool is_control_character(char c);
	DLLUDM bool does_key_require_quotes(const std::string_view &key);

	struct DLLUDM AsciiException
		: public Exception
	{
		AsciiException(const std::string &msg,uint32_t lineIdx,uint32_t charIdx);
		uint32_t lineIndex = 0;
		uint32_t charIndex = 0;
	};

	struct DLLUDM SyntaxError : public AsciiException {using AsciiException::AsciiException;};
	struct DLLUDM DataError : public AsciiException {using AsciiException::AsciiException;};

	constexpr const char *enum_type_to_ascii(Type t)
	{
		// Note: These have to match ascii_type_to_enum
		switch(t)
		{
		case Type::Nil:
			return "nil";
		case Type::String:
			return "string";
		case Type::Utf8String:
			return "utf8";
		case Type::Int8:
			return "int8";
		case Type::UInt8:
			return "uint8";
		case Type::Int16:
			return "int16";
		case Type::UInt16:
			return "uint16";
		case Type::Int32:
			return "int32";
		case Type::UInt32:
			return "uint32";
		case Type::Int64:
			return "int64";
		case Type::UInt64:
			return "uint64";
		case Type::Float:
			return "float";
		case Type::Double:
			return "double";
		case Type::Boolean:
			return "bool";
		case Type::Vector2:
			return "vec2";
		case Type::Vector2i:
			return "vec2i";
		case Type::Vector3:
			return "vec3";
		case Type::Vector3i:
			return "vec3i";
		case Type::Vector4:
			return "vec4";
		case Type::Vector4i:
			return "vec4i";
		case Type::Quaternion:
			return "quat";
		case Type::EulerAngles:
			return "ang";
		case Type::Srgba:
			return "srgba";
		case Type::HdrColor:
			return "hdr";
		case Type::Transform:
			return "transform";
		case Type::ScaledTransform:
			return "stransform";
		case Type::Mat4:
			return "mat4";
		case Type::Mat3x4:
			return "mat3x4";
		case Type::Blob:
			return "blob";
		case Type::BlobLz4:
			return "lz4";
		case Type::Array:
			return "array";
		case Type::ArrayLz4:
			return "arrayLz4";
		case Type::Element:
			return "element";
		case Type::Reference:
			return "ref";
		case Type::Half:
			return "half";
		case Type::Struct:
			return "struct";
		}
		static_assert(umath::to_integral(Type::Count) == 36,"Update this list when new types are added!");
	}
	DLLUDM Type ascii_type_to_enum(const std::string_view &type);
	DLLUDM void sanitize_key_name(std::string &key);

	DLLUDM Blob decompress_lz4_blob(const BlobLz4 &data);
	DLLUDM Blob decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize);
	DLLUDM void decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize,void *outData);
	DLLUDM BlobLz4 compress_lz4_blob(const Blob &data);
	DLLUDM BlobLz4 compress_lz4_blob(const void *data,uint64_t size);
	template<class T>
		BlobLz4 compress_lz4_blob(const T &v)
	{
		return compress_lz4_blob(v.data(),v.size() *sizeof(v[0]));
	}

	using IFile = ufile::IFile;
	using MemoryFile = ufile::MemoryFile;
	using VectorFile = ufile::VectorFile;
	
	struct DLLUDM Property
	{
		template<typename T>
			static PProperty Create(T &&value);
		template<typename T>
			static PProperty Create();
		static PProperty Create() {return Create<Nil>();}
		static PProperty Create(Type type);
		Property()=default;
		Property(const Property &other);
		Property(Property &&other);
		~Property();
		bool IsType(Type ptype) const {return ptype == type;}
		Property &operator=(const Property &other);
		Property &operator=(Property &&other)=delete;
		template<typename T>
			void operator=(T &&v);
		void Copy(const Property &other,bool deepCopy);
		PProperty Copy(bool deepCopy=false) const;

		Type type = Type::Nil;
		DataValue value = nullptr;

		LinkedPropertyWrapper operator[](const std::string &key);
		LinkedPropertyWrapper operator[](const char *key);

		bool operator==(const Property &other) const;
		bool operator!=(const Property &other) const {return !operator==(other);}

		// operator udm::LinkedPropertyWrapper();

		bool Compress();
		bool Decompress(const std::optional<Type> arrayValueType={});

		BlobResult GetBlobData(void *outBuffer,size_t bufferSize,uint64_t *optOutRequiredSize=nullptr) const;
		BlobResult GetBlobData(void *outBuffer,size_t bufferSize,Type type,uint64_t *optOutRequiredSize=nullptr) const;
		Blob GetBlobData(Type &outType) const;
		template<class T>
			BlobResult GetBlobData(T &v) const;
		template<typename T>
			T &GetValue();
		template<typename T>
			const T &GetValue() const;
		template<typename T>
			T *GetValuePtr();
		void *GetValuePtr(Type &outType);
		template<typename T>
			T ToValue(const T &defaultValue) const;
		template<typename T>
			std::optional<T> ToValue() const;
		operator bool() const {return type != Type::Nil;}
		
		bool Read(IFile &f);
		bool Read(Type type,IFile &f);
		void Write(IFile &f) const;

		void ToAscii(AsciiSaveFlags flags,std::stringstream &ss,const std::string &propName,const std::string &prefix="");
		
		static void ToAscii(AsciiSaveFlags flags,std::stringstream &ss,const std::string &propName,Type type,const DataValue value,const std::string &prefix="");
		bool Read(IFile &f,Blob &outBlob);
		bool Read(IFile &f,BlobLz4 &outBlob);
		bool Read(IFile &f,Utf8String &outStr);
		bool Read(IFile &f,Element &outEl);
		bool Read(IFile &f,Array &outArray);
		bool Read(IFile &f,ArrayLz4 &outArray);
		static bool Read(IFile &f,String &outStr);
		bool Read(IFile &f,Reference &outRef);
		bool Read(IFile &f,Struct &strct);
		static void Write(IFile &f,const Blob &blob);
		static void Write(IFile &f,const BlobLz4 &blob);
		static void Write(IFile &f,const Utf8String &str);
		static void Write(IFile &f,const Element &el);
		static void Write(IFile &f,const Array &a);
		static void Write(IFile &f,const ArrayLz4 &a);
		static void Write(IFile &f,const String &str);
		static void Write(IFile &f,const Reference &ref);
		static void Write(IFile &f,const Struct &strct);
		
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Nil &nil,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Blob &blob,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const BlobLz4 &blob,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Utf8String &utf8,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Element &el,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Array &a,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const ArrayLz4 &a,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const String &str,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Reference &ref,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Struct &strct,const std::string &prefix="");
		static std::string StructToAsciiValue(AsciiSaveFlags flags,const StructDescription &strct,const void *data,const std::string &prefix="");
		static void ArrayValuesToAscii(AsciiSaveFlags flags,std::stringstream &ss,const Array &a,const std::string &prefix="");
		
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Vector2 &v,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Vector2i &v,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Vector3 &v,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Vector3i &v,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Vector4 &v,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Vector4i &v,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Quaternion &q,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const EulerAngles &a,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Srgba &srgb,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const HdrColor &col,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Transform &t,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const ScaledTransform &t,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Mat4 &m,const std::string &prefix="");
		static std::string ToAsciiValue(AsciiSaveFlags flags,const Mat3x4 &m,const std::string &prefix="");

		static constexpr uint8_t EXTENDED_STRING_IDENTIFIER = std::numeric_limits<uint8_t>::max();
		static BlobResult GetBlobData(const Blob &blob,void *outBuffer,size_t bufferSize);
		static BlobResult GetBlobData(const BlobLz4 &blob,void *outBuffer,size_t bufferSize);
		static Blob GetBlobData(const BlobLz4 &blob);
		static uint32_t GetStringPrefixSizeRequirement(const String &str);
		static uint32_t GetStringSizeRequirement(const String &str);
	private:
		friend PropertyWrapper;
		bool ReadStructHeader(IFile &f,StructDescription &strct);
		static void WriteStructHeader(IFile &f,const StructDescription &strct);
		template<bool ENABLE_EXCEPTIONS,typename T>
			bool Assign(T &&v);

		template<typename T>
			static uint64_t WriteBlockSize(IFile &f);
		template<typename T>
			static void WriteBlockSize(IFile &f,uint64_t offset);

		template<typename T>
			static void NumericTypeToString(T value,std::stringstream &ss);
		template<typename T>
			static std::string NumericTypeToString(T value);
		static void SetAppropriatePrecision(std::stringstream &ss,Type type);
		static void RemoveTrailingZeroes(std::string &str);
		void Initialize();
		void Clear();
		template<typename T>
			T &GetValue(Type type);
	};

	using Version = uint32_t;
	/* Version history:
	* 1: Initial version
	* 2: Added types: reference, arrayLz4, struct, half, vector2i, vector3i, vector4i
	*/
	static constexpr Version VERSION = 2;
	static constexpr auto *HEADER_IDENTIFIER = "UDMB";
#pragma pack(push,1)
	struct DLLUDM Header
	{
		Header()=default;
		std::array<char,4> identifier = {HEADER_IDENTIFIER[0],HEADER_IDENTIFIER[1],HEADER_IDENTIFIER[2],HEADER_IDENTIFIER[3]};
		Version version = VERSION;
	};
#pragma pack(pop)

	struct DLLUDM AssetData
		: public LinkedPropertyWrapper
	{
		std::string GetAssetType() const;
		Version GetAssetVersion() const;
		void SetAssetType(const std::string &assetType) const;
		void SetAssetVersion(Version version) const;

		LinkedPropertyWrapper GetData() const;
		LinkedPropertyWrapper operator*() const {return GetData();}
		LinkedPropertyWrapper operator->() const {return GetData();}
	};

	class DLLUDM Data
	{
	public:
		static constexpr auto KEY_ASSET_TYPE = "assetType";
		static constexpr auto KEY_ASSET_VERSION = "assetVersion";
		static constexpr auto KEY_ASSET_DATA = "assetData";
		static std::optional<FormatType> GetFormatType(const std::string &fileName,std::string &outErr);
		static std::optional<FormatType> GetFormatType(std::unique_ptr<IFile> &&f,std::string &outErr);
		static std::optional<FormatType> GetFormatType(const ::VFilePtr &f,std::string &outErr);
		static std::shared_ptr<Data> Load(const std::string &fileName);
		static std::shared_ptr<Data> Load(std::unique_ptr<IFile> &&f);
		static std::shared_ptr<Data> Load(const ::VFilePtr &f);
		static std::shared_ptr<Data> Open(const std::string &fileName);
		static std::shared_ptr<Data> Open(std::unique_ptr<IFile> &&f);
		static std::shared_ptr<Data> Open(const ::VFilePtr &f);
		static std::shared_ptr<Data> Create(const std::string &assetType,Version assetVersion);
		static std::shared_ptr<Data> Create();
		static bool DebugTest();

		PProperty LoadProperty(const std::string_view &path) const;
		void ResolveReferences();

		bool Save(const std::string &fileName) const;
		bool Save(IFile &f) const;
		bool Save(const ::VFilePtr &f);
		bool SaveAscii(const std::string &fileName,AsciiSaveFlags flags=AsciiSaveFlags::None) const;
		bool SaveAscii(IFile &f,AsciiSaveFlags flags=AsciiSaveFlags::None) const;
		bool SaveAscii(const ::VFilePtr &f,AsciiSaveFlags flags=AsciiSaveFlags::None) const;
		Element &GetRootElement() {return *static_cast<Element*>(m_rootProperty->value);}
		const Element &GetRootElement() const {return const_cast<Data*>(this)->GetRootElement();}
		AssetData GetAssetData() const;

		bool operator==(const Data &other) const;
		bool operator!=(const Data &other) const {return !operator==(other);}
		
		LinkedPropertyWrapper operator[](const std::string &key) const;
		Element *operator->();
		const Element *operator->() const;
		Element &operator*();
		const Element &operator*() const;

		std::string GetAssetType() const;
		Version GetAssetVersion() const;
		void SetAssetType(const std::string &assetType);
		void SetAssetVersion(Version version);

		void ToAscii(std::stringstream &ss,AsciiSaveFlags flags=AsciiSaveFlags::None) const;

		const Header &GetHeader() const {return m_header;}

		static std::string ReadKey(IFile &f);
		static void WriteKey(IFile &f,const std::string &key);
	private:
		friend AsciiReader;
		friend ArrayLz4;
		bool ValidateHeaderProperties();
		static void SkipProperty(IFile &f,Type type);
		PProperty LoadProperty(Type type,const std::string_view &path) const;
		static PProperty ReadProperty(IFile &f);
		static void WriteProperty(IFile &f,const Property &o);
		Data()=default;
		Header m_header;
		std::unique_ptr<IFile> m_file = nullptr;
		PProperty m_rootProperty = nullptr;
	};

	template<typename TEnum>
		constexpr std::string_view enum_to_string(TEnum e)
	{
		return magic_enum::enum_name(e);
	}

	template<typename TEnum>
		constexpr std::string flags_to_string(TEnum e)
	{
		return magic_enum::flags::enum_name(e);
	}

	template<typename TEnum>
		static TEnum string_to_enum(udm::LinkedPropertyWrapperArg udmEnum,TEnum def)
	{
		std::string str;
		udmEnum(str);
		auto e = magic_enum::enum_cast<TEnum>(str);
		return e.has_value() ? *e : def;
	}

	template<typename TEnum>
		static TEnum string_to_flags(udm::LinkedPropertyWrapperArg udmEnum,TEnum def)
	{
		std::string str;
		udmEnum(str);
		auto e = magic_enum::flags::enum_cast<TEnum>(str);
		return e.has_value() ? *e : def;
	}

	template<typename TEnum>
		static void to_enum_value(udm::LinkedPropertyWrapperArg udmEnum,TEnum &def)
	{
		std::string str;
		udmEnum(str);
		auto e = magic_enum::enum_cast<TEnum>(str);
		def = e.has_value() ? *e : def;
	}

	template<typename TEnum>
		static void to_flags(udm::LinkedPropertyWrapperArg udmEnum,TEnum &def)
	{
		std::string str;
		udmEnum(str);
		auto e = magic_enum::flags::enum_cast<TEnum>(str);
		def = e.has_value() ? *e : def;
	}

	template<typename TEnum>
		void write_flag(udm::LinkedPropertyWrapperArg udm,TEnum flags,TEnum flag,const std::string_view &name)
	{
		if(umath::is_flag_set(flags,flag) == false)
			return;
		udm[name] = true;
	}
	template<typename TEnum>
		void read_flag(LinkedPropertyWrapperArg udm,TEnum &flags,TEnum flag,const std::string_view &name)
	{
		if(!udm)
			return;
		umath::set_flag(flags,flag,udm[name](false));
	}

	void to_json(LinkedPropertyWrapperArg prop,std::stringstream &ss);

	namespace detail
	{
		DLLUDM void test_c_wrapper();
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(udm::AsciiSaveFlags)
REGISTER_BASIC_BITWISE_OPERATORS(udm::MergeFlags)

template<bool ENABLE_EXCEPTIONS,typename T>
	bool udm::Property::Assign(T &&v)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if constexpr(util::is_specialization<TBase,std::vector>::value)
	{
		using TValueType = typename TBase::value_type;
		if(!is_array_type(type))
		{
			if constexpr(ENABLE_EXCEPTIONS)
				throw InvalidUsageError{"Attempted to assign vector to non-array property (of type " +std::string{magic_enum::enum_name(type)} +"), this is not allowed!"};
			else
				return false;
		}
		auto valueType = type_to_enum<TValueType>();
		auto size = v.size();
		auto &a = *static_cast<Array*>(value);
		a.Clear();
		a.SetValueType(valueType);
		a.Resize(size);

		if(size_of_base_type(valueType) != sizeof(typename TBase::value_type))
		{
			if constexpr(ENABLE_EXCEPTIONS)
				throw InvalidUsageError{"Type size mismatch!"};
			else
				return false;
		}
		auto vs = [this,&a,&v](auto tag) {
			using TTag = decltype(tag)::type;
			memcpy(a.GetValues(),v.data(),v.size() *sizeof(v[0]));
		};
		if(is_ng_type(valueType))
			visit_ng(valueType,vs);
		else if(is_non_trivial_type(valueType))
		{
			// Elements have to be copied explicitly
			for(auto i=decltype(size){0u};i<size;++i)
				a[i] = v[i];
		}
		else
			return false;
		return true;
	}
	else if constexpr(util::is_specialization<TBase,std::unordered_map>::value || util::is_specialization<TBase,std::map>::value)
	{
		if(type != Type::Element)
		{
			if constexpr(ENABLE_EXCEPTIONS)
				throw InvalidUsageError{"Attempted to assign map to non-element property (of type " +std::string{magic_enum::enum_name(type)} +"), this is not allowed!"};
			else
				return false;
		}
		for(auto &pair : v)
			(*this)[pair.first] = pair.second;
		return true;
	}
	auto vType = type_to_enum_s<TBase>();
	if(vType == Type::Invalid)
	{
		if constexpr(ENABLE_EXCEPTIONS)
			throw LogicError{"Attempted to assign value of type '" +std::string{typeid(T).name()} +"', which is not a recognized type!"};
		else
			return false;
	}
	auto vs = [this,&v](auto tag) {
		using TTag = decltype(tag)::type;
		if constexpr(is_convertible<TBase,TTag>())
		{
			*static_cast<TTag*>(value) = convert<TBase,TTag>(v);
			return true;
		}
		else
			return false;
	};
	return visit(vType,vs);
}

template<typename T>
	void udm::Property::operator=(T &&v)
{
	Assign<true,T>(v);
}

template<typename T>
	void udm::LinkedPropertyWrapper::operator=(T &&v) const
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if(prop == nullptr)
		const_cast<LinkedPropertyWrapper*>(this)->InitializeProperty();
	/*if(prev && prev->arrayIndex != std::numeric_limits<uint32_t>::max() && prev->prev && prev->prev->prop && prev->prev->prop->type == Type::Array)
	{
		(*static_cast<Array*>(prev->prev->prop->value))[prev->arrayIndex][propName] = v;
		return;
	}*/
	PropertyWrapper::operator=(v);
}
template<typename T>
	void udm::PropertyWrapper::operator=(T &&v) const
{
	if(prop == nullptr)
		throw LogicError{"Cannot assign property value: Property is invalid!"};
	if constexpr(std::is_enum_v<std::remove_reference_t<T>>)
		return operator=(magic_enum::enum_name(v));
	else
	{
		using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
		if(is_array_type(prop->type))
		{
			if(arrayIndex == std::numeric_limits<uint32_t>::max())
				throw LogicError{"Cannot assign propety value to array: No index has been specified!"};
			if(linked && !static_cast<const LinkedPropertyWrapper*>(this)->propName.empty())
			{
				auto &a = *static_cast<Array*>(prop->value);
				if(a.GetValueType() != Type::Element)
					return;
				auto &e = a.GetValue<Element>(arrayIndex);
				if constexpr(type_to_enum_s<TBase>() != Type::Invalid)
					e.children[static_cast<const LinkedPropertyWrapper*>(this)->propName] = Property::Create(v);
				else if constexpr(std::is_same_v<TBase,PProperty>)
					e.children[static_cast<const LinkedPropertyWrapper*>(this)->propName] = v;
				else if constexpr(std::is_same_v<TBase,Property>)
					e.children[static_cast<const LinkedPropertyWrapper*>(this)->propName] = std::make_shared<Property>(v);
				else
				{
					auto it = e.children.find(static_cast<const LinkedPropertyWrapper*>(this)->propName);
					if(it == e.children.end() || it->second->IsType(Type::Struct) == false)
						throw LogicError{"Cannot assign custom type to non-struct property!"};
					it->second->GetValue<Struct>() = std::move(v);
				}
			}
			else
				static_cast<Array*>(prop->value)->SetValue(arrayIndex,v);
			return;
		}
		if constexpr(std::is_same_v<TBase,PProperty>)
		{
			if(linked && !static_cast<const LinkedPropertyWrapper*>(this)->propName.empty())
			{
				auto &linked = *GetLinked();
				if(linked.prev && linked.prev->IsType(Type::Element))
				{
					auto &el = linked.prev->GetValue<Element>();
					el.children[linked.propName] = v;
					return;
				}
			}
		}
		if(prop->type != Type::Element)
		{
			*prop = v;
			return;
		}
		if constexpr(type_to_enum_s<TBase>() != Type::Invalid)
		{
			if(prop->value == nullptr)
				throw LogicError{"Cannot assign property value: Property is invalid!"};
			auto &el = *static_cast<Element*>(prop->value);
			auto &wpParent = el.parentProperty;
			if(!wpParent)
				throw InvalidUsageError{"Attempted to change value of element property without a valid parent, this is not allowed!"};
			auto &parent = wpParent;
			switch(parent->type)
			{
			case Type::Element:
				static_cast<Element*>(parent->value)->SetValue(el,v);
				break;
			/*case Type::Array:
				if(arrayIndex == std::numeric_limits<uint32_t>::max())
					throw std::runtime_error{"Element has parent of type " +std::string{magic_enum::enum_name(parent->type)} +", but is not indexed!"};
				(*static_cast<Array*>(parent->value))[arrayIndex] = v;
				break;*/
			default:
				throw InvalidUsageError{
					"Element has parent of type " +std::string{magic_enum::enum_name(parent->type)} +", but only " +std::string{magic_enum::enum_name(Type::Element)}/* +" and " +std::string{magic_enum::enum_name(Type::Array)}*/ +" types are allowed!"
				};
			}
		}
		else
			throw LogicError{"Cannot assign custom type to non-struct property!"};
	}
}

template<typename T>
	udm::PProperty udm::Property::Create(T &&value)
{
	auto prop = Create<T>();
	*prop = value;
	return prop;
}

template<typename T>
	udm::PProperty udm::Property::Create()
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	return Create(type_to_enum<TBase>());
}

template<typename T>
	uint64_t udm::Property::WriteBlockSize(IFile &f)
{
	auto offsetToSize = f.Tell();
	f.Write<T>(0);
	return offsetToSize;
}
template<typename T>
	void udm::Property::WriteBlockSize(IFile &f,uint64_t offset)
{
	auto startOffset = offset +sizeof(T);
	auto curOffset = f.Tell();
	f.Seek(offset);
	f.Write<T>(curOffset -startOffset);
	f.Seek(curOffset);
}

template<typename T>
	void udm::Property::NumericTypeToString(T value,std::stringstream &ss)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if constexpr(std::is_same_v<TBase,Half>)
	{
		NumericTypeToString<float>(value,ss);
		return;
	}
	if constexpr(!std::is_floating_point_v<T>)
	{
		if constexpr(std::is_same_v<T,Int8> || std::is_same_v<T,UInt8>)
			ss<<+value;
		else
			ss<<value;
		return;
	}
	// SetAppropriatePrecision(ss,type_to_enum<T>());
	ss<<NumericTypeToString(value);
}
template<typename T>
	std::string udm::Property::NumericTypeToString(T value)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if constexpr(std::is_same_v<TBase,Half>)
		return NumericTypeToString<float>(value);
	if constexpr(!std::is_floating_point_v<T>)
	{
		if constexpr(std::is_same_v<T,Int8> || std::is_same_v<T,UInt8>)
			return std::to_string(+value);
		return std::to_string(value);
	}
	// TODO: This is not very efficient...
	// (We need a temporary stringstream because we want to
	// remove trailing zeroes)
	std::stringstream tmp;
	SetAppropriatePrecision(tmp,type_to_enum<T>());
	if constexpr(std::is_same_v<T,Int8> || std::is_same_v<T,UInt8>)
		tmp<<+value;
	else
		tmp<<value;
	auto str = tmp.str();
	RemoveTrailingZeroes(str);
	return str;
}

template<typename T>
	T *udm::Array::GetValuePtr(uint32_t idx)
{
	if(type_to_enum<T>() != m_valueType)
		return nullptr;
	return &GetValue<T>(idx);
}

template<typename T>
	T &udm::Array::GetValue(uint32_t idx)
{
	if(idx >= m_size)
		throw OutOfBoundsError{"Array index " +std::to_string(idx) +" out of bounds of array of size " +std::to_string(m_size) +"!"};
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	auto vs = [this,idx](auto tag) -> T& {
		using TTag = decltype(tag)::type;
		if constexpr(std::is_same_v<TTag,TBase>)
			return static_cast<TTag*>(GetValues())[idx];
		throw LogicError{"Attempted to retrieve value of type " +std::string{magic_enum::enum_name(type_to_enum<T>())} +" from array of type " +std::string{magic_enum::enum_name(m_valueType)} +"!"};
	};
	auto valueType = GetValueType();
	return visit(valueType,vs);
}

template<typename T>
	void udm::Array::InsertValue(uint32_t idx,T &&value)
{
	auto size = GetSize();
	if(idx > size)
		return;
	Range r0 {0 /* src */,0 /* dst */,idx};
	Range r1 {idx /* src */,idx +1 /* dst */,size -idx};
	Resize(size +1,r0,r1,false);
	if constexpr(std::is_rvalue_reference_v<T>)
		(*this)[idx] = std::move(value);
	else
		(*this)[idx] = value;
}

template<typename T>
	void udm::Array::SetValue(uint32_t idx,T &&v)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	auto valueType = GetValueType();
	if(valueType == Type::Struct)
	{
		if constexpr(!std::is_fundamental_v<std::remove_extent_t<TBase>>)
		{
			auto sz = GetStructuredDataInfo()->GetDataSizeRequirement();
			if(sizeof(T) != sz)
				throw LogicError{"Attempted to assign data of size " +std::to_string(sizeof(T)) +" to struct of size " +std::to_string(sz) +"!"};
			if constexpr(std::is_rvalue_reference_v<T>)
				static_cast<TBase*>(GetValues())[idx] = std::move(v);
			else
				static_cast<TBase*>(GetValues())[idx] = v;
		}
		else
			throw LogicError{"Attempted to assign fundamental type '" +std::string{typeid(T).name()} +"' to struct, this is not allowed!"};
		return;
	}
	if(!is_convertible<TBase>(valueType))
	{
		throw LogicError{
			"Attempted to insert value of type " +std::string{magic_enum::enum_name(type_to_enum_s<TBase>())} +" into array of type " +std::string{magic_enum::enum_name(valueType)} +", which are not compatible!"
		};
	}

	auto vs = [this,idx,&v](auto tag) {
		using TTag = decltype(tag)::type;
		if constexpr(is_convertible<TBase,TTag>())
			static_cast<TTag*>(GetValues())[idx] = convert<TBase,TTag>(v);
	};
	visit(valueType,vs);
}

template<typename T>
	void udm::Element::SetValue(Element &child,T &&v)
{
	auto it = std::find_if(children.begin(),children.end(),[&child](const std::pair<std::string,PProperty> &pair) {
		return pair.second->type == udm::Type::Element && pair.second->value == &child;
	});
	if(it == children.end())
		return;
	children[it->first] = Property::Create<T>(std::forward<T>(v));
}

template<typename T>
	T &udm::Property::GetValue()
{
	return GetValue<T>(type_to_enum<T>());
}

template<typename T>
	const T &udm::Property::GetValue() const
{
	return const_cast<Property*>(this)->GetValue<T>();
}
template<typename T>
	T udm::Property::ToValue(const T &defaultValue) const
{
	if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
		return defaultValue;
	auto val = ToValue<T>();
	return val.has_value() ? *val : defaultValue;
}
template<typename T>
	std::optional<T> udm::Property::ToValue() const
{
	if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
		return {};
	if constexpr(util::is_specialization<T,std::vector>::value)
	{
		T v {};
		auto res = GetBlobData(v);
		return (res == BlobResult::Success) ? v : std::optional<T>{};
	}
	else if constexpr(util::is_specialization<T,std::unordered_map>::value || util::is_specialization<T,std::map>::value)
	{
		if(type != Type::Element)
			return {};
		using TValue = decltype(T::value_type::second);
		auto &e = GetValue<Element>();
		T result {};
		for(auto &pair : e.children)
		{
			auto val = pair.second->ToValue<TValue>();
			if(val.has_value() == false)
				continue;
			result[pair.first] = std::move(val.value());
		}
		return result;
	}
	auto vs = [&](auto tag) -> std::optional<T> {
		if constexpr(is_convertible<typename decltype(tag)::type,T>())
			return convert<typename decltype(tag)::type,T>(const_cast<udm::Property*>(this)->GetValue<typename decltype(tag)::type>());
		return {};
	};
	return visit(type,vs);
	static_assert(umath::to_integral(Type::Count) == 36,"Update this list when new types are added!");
	return {};
}

template<typename T>
	T &udm::Property::GetValue(Type type)
{
	assert(value && this->type == type);
	if(this->type != type && !(this->type == Type::ArrayLz4 && type == Type::Array))
		throw LogicError{"Type mismatch, requested type is " +std::string{magic_enum::enum_name(type)} +", but actual type is " +std::string{magic_enum::enum_name(this->type)} +"!"};
	return *GetValuePtr<T>();
}

template<typename T>
	T *udm::Property::GetValuePtr()
{
	// TODO: this should never be null, but there are certain cases where it seems to happen
	if(!this)
		return nullptr;
	if constexpr(std::is_same_v<T,udm::Array>)
		return is_array_type(this->type) ? reinterpret_cast<T*>(value) : nullptr;
	return (this->type == type_to_enum<T>()) ? reinterpret_cast<T*>(value) : nullptr;
}

template<typename T>
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,const StructDescription &strct,const T *data,uint32_t strctItems,ArrayType arrayType,bool pathToElements) const
{
	auto prop = AddArray(path,strct,strctItems,arrayType,pathToElements);
	auto &a = prop.template GetValue<Array>();
	auto sz = a.GetValueSize() *a.GetSize();
	auto *ptr = a.GetValues();
	memcpy(ptr,data,sz);
	return prop;
}

template<typename T>
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,const StructDescription &strct,const std::vector<T> &values,ArrayType arrayType,bool pathToElements) const
{
	auto prop = AddArray(path,strct,values.size(),arrayType,pathToElements);
	auto &a = prop.template GetValue<Array>();
	auto sz = a.GetValueSize() *a.GetSize();
	auto szValues = util::size_of_container(values);
	if(szValues != sz)
		throw InvalidUsageError{"Size of values does not match expected size of defined struct!"};
	auto *ptr = a.GetValues();
	memcpy(ptr,values.data(),szValues);
	return prop;
}

template<typename T>
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,const std::vector<T> &values,ArrayType arrayType,bool pathToElements) const
{
	return AddArray<T>(path,values.size(),values.data(),arrayType,pathToElements);
}

template<typename T>
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,uint32_t size,const T *data,ArrayType arrayType,bool pathToElements) const
{
	constexpr auto valueType = type_to_enum<T>();
	auto prop = AddArray(path,size,valueType,arrayType,pathToElements);
	auto &a = prop.template GetValue<Array>();
	if constexpr(is_non_trivial_type(valueType) && valueType != Type::Struct)
	{
		for(auto i=decltype(size){0u};i<size;++i)
			a[i] = data[i];
	}
	else
		memcpy(a.GetValues(),data,sizeof(T) *size);
	return prop;
}

template<class T>
	udm::BlobResult udm::Property::GetBlobData(T &v) const
{
	if(!*this)
		return BlobResult::InvalidProperty;
	uint64_t reqBufferSize = 0;
	auto result = GetBlobData(v.data(),v.size() *sizeof(v[0]),&reqBufferSize);
	if(result == BlobResult::InsufficientSize)
	{
		if(v.size() *sizeof(v[0]) != reqBufferSize)
		{
			if((reqBufferSize %sizeof(v[0])) > 0)
				return BlobResult::ValueTypeMismatch;
			v.resize(reqBufferSize /sizeof(v[0]));
			return GetBlobData<T>(v);
		}
		return result;
	}
	if(result != BlobResult::NotABlobType)
		return result;
	if constexpr(is_trivial_type(type_to_enum_s<typename T::value_type>()))
	{
		if(is_array_type(this->type))
		{
			auto &a = GetValue<Array>();
			if(a.GetValueType() == type_to_enum<typename T::value_type>())
			{
				v.resize(a.GetSize());
				memcpy(v.data(),a.GetValues(),v.size() *sizeof(v[0]));
				return BlobResult::Success;
			}
		}
	}
	return BlobResult::NotABlobType;
}

template<class T>
	udm::BlobResult udm::PropertyWrapper::GetBlobData(T &v) const
{
	uint64_t reqBufferSize = 0;
	auto result = GetBlobData(v.data(),v.size() *sizeof(v[0]),&reqBufferSize);
	if(result == BlobResult::InsufficientSize)
	{
		if(v.size() *sizeof(v[0]) != reqBufferSize)
		{
			if((reqBufferSize %sizeof(v[0])) > 0)
				return BlobResult::ValueTypeMismatch;
			v.resize(reqBufferSize /sizeof(v[0]));
			return GetBlobData<T>(v);
		}
		return result;
	}
	if(result != BlobResult::NotABlobType)
		return result;
	if(IsArrayItem(true))
	{
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
		{
			auto &a = prop->GetValue<Array>();
			auto *el = a.GetValuePtr<Element>(arrayIndex);
			if(!el)
				return BlobResult::InvalidProperty;
			auto it = el->children.find(static_cast<const LinkedPropertyWrapper&>(*this).propName);
			if(it != el->children.end())
				return it->second->GetBlobData<T>(v);
			return BlobResult::InvalidProperty;
		}
		return BlobResult::NotABlobType;
	}
	return (*this)->GetBlobData(v);
}
template<typename T>
	T &udm::PropertyWrapper::GetValue() const
{
	if(arrayIndex != std::numeric_limits<uint32_t>::max())
	{
		auto *a = prop->GetValuePtr<Array>();
		if(a)
		{
			if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
				return const_cast<Element&>(a->GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValue<T>();
			if(a->IsValueType(type_to_enum<T>()) == false)
				throw LogicError{"Type mismatch, requested type is " +std::string{magic_enum::enum_name(type_to_enum<T>())} +", but actual type is " +std::string{magic_enum::enum_name(a->GetValueType())} +"!"};
			return static_cast<T*>(a->GetValues())[arrayIndex];
		}
	}
	return (*this)->GetValue<T>();
}

template<typename T>
	T *udm::PropertyWrapper::GetValuePtr() const
{
	if(arrayIndex != std::numeric_limits<uint32_t>::max())
	{
		auto *a = prop->GetValuePtr<Array>();
		if(a)
		{
			if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
				return const_cast<Element&>(a->GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValuePtr<T>();
			if(a->IsValueType(type_to_enum<T>()) == false)
				return nullptr;
			return &static_cast<T*>(a->GetValues())[arrayIndex];
		}
	}
	return prop ? (*this)->GetValuePtr<T>() : nullptr;
}
template<typename T>
	T udm::PropertyWrapper::ToValue(const T &defaultValue,bool *optOutIsDefined) const
{
	if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
	{
		if(optOutIsDefined)
			*optOutIsDefined = false;
		return defaultValue;
	}
	auto val = ToValue<T>();
	if(val.has_value())
	{
		if(optOutIsDefined)
			*optOutIsDefined = true;
		return std::move(val.value());
	}
	if(optOutIsDefined)
		*optOutIsDefined = false;
	return defaultValue;
}

template<typename T>
	bool udm::PropertyWrapper::operator==(const T &other) const
{
	if constexpr(util::is_c_string<T>())
		return operator==(std::string{other});
	else
	{
		auto *val = GetValuePtr<T>();
		if(val)
			return *val == other;
		auto valConv = ToValue<T>();
		return valConv.has_value() ? *valConv == other : false;
	}
	return false;
}

template<typename T>
	udm::ArrayIterator<T> udm::PropertyWrapper::begin() const
{
	if(!static_cast<bool>(*this))
		return ArrayIterator<T>{};
	auto *a = GetValuePtr<Array>();
	if(a == nullptr)
		return ArrayIterator<T>{};
	auto it = a->begin<T>();
	if(linked)
		it.GetProperty().prev = std::make_unique<LinkedPropertyWrapper>(*static_cast<LinkedPropertyWrapper*>(const_cast<PropertyWrapper*>(this)));
	return it;
}
template<typename T>
	udm::ArrayIterator<T> udm::PropertyWrapper::end() const
{
	if(!static_cast<bool>(*this))
		return ArrayIterator<T>{};
	auto *a = GetValuePtr<Array>();
	if(a == nullptr)
		return ArrayIterator<T>{};
	return a->end<T>();
}

template<typename T>
	std::optional<T> udm::PropertyWrapper::ToValue() const
{
	if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
		return {};
	if(IsArrayItem(true))
	{
		auto &a = prop->GetValue<Array>();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->ToValue<T>();
		auto vs = [&](auto tag) -> std::optional<T> {
			if constexpr(is_convertible<typename decltype(tag)::type,T>())
				return std::optional<T>{convert<typename decltype(tag)::type,T>(const_cast<udm::PropertyWrapper*>(this)->GetValue<typename decltype(tag)::type>())};
			return {};
		};
		auto valueType = a.GetValueType();
		return visit(valueType,vs);
	}
	return (*this)->ToValue<T>();
}

template<typename T>
	udm::ArrayIterator<T>::ArrayIterator()
	: m_curProperty{}
{}

template<typename T>
	udm::ArrayIterator<T>::ArrayIterator(udm::Array &a,uint32_t idx)
	: m_curProperty{a,idx}
{}

template<typename T>
	udm::ArrayIterator<T>::ArrayIterator(udm::Array &a)
	: ArrayIterator{a,0u}
{}

template<typename T>
	udm::ArrayIterator<T>::ArrayIterator(const ArrayIterator &other)
	: m_curProperty{other.m_curProperty}
{}

template<typename T>
	udm::ArrayIterator<T> &udm::ArrayIterator<T>::operator++()
{
	++m_curProperty.arrayIndex;
	return *this;
}

template<typename T>
	udm::ArrayIterator<T> udm::ArrayIterator<T>::operator++(int)
{
	auto it = *this;
	it.operator++();
	return it;
}

template<typename T>
	udm::ArrayIterator<T> udm::ArrayIterator<T>::operator+(uint32_t n)
{
	auto it = *this;
	for(auto i=decltype(n){0u};i<n;++i)
		it.operator++();
	return it;
}

template<typename T>
	typename udm::ArrayIterator<T>::reference udm::ArrayIterator<T>::operator*()
	{
		if constexpr(std::is_same_v<T,udm::LinkedPropertyWrapper>)
			return m_curProperty;
		else
			return m_curProperty.GetValue<T>();
	}

template<typename T>
	typename udm::ArrayIterator<T>::pointer udm::ArrayIterator<T>::operator->()
	{
		if constexpr(std::is_same_v<T,udm::LinkedPropertyWrapper>)
			return &m_curProperty;
		else
			return m_curProperty.GetValuePtr<T>();
	}

template<typename T>
	bool udm::ArrayIterator<T>::operator==(const ArrayIterator &other) const
{
	auto res = (m_curProperty == other.m_curProperty);
	// UDM_ASSERT_COMPARISON(res);
	return res;
}

template<typename T>
	bool udm::ArrayIterator<T>::operator!=(const ArrayIterator &other) const {return !operator==(other);}

template<class T>
	udm::Struct &udm::Struct::operator=(const T &other)
{
	auto sz = description.GetDataSizeRequirement();
	if(sizeof(T) != sz)
		throw LogicError{"Attempted to assign data of size " +std::to_string(sizeof(T)) +" to struct of size " +std::to_string(sz) +"!"};
	if(data.size() != sz)
		throw ImplementationError{"Size of struct data does not match its types!"};
	memcpy(data.data(),&other,sizeof(T));
	return *this;
}
#pragma warning( pop )

#endif
