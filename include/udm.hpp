/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_HPP__
#define __UDM_HPP__

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
#include <fsys/filesystem.h>
#include "udm_types.hpp"
#include "udm_trivial_types.hpp"
#include "udm_conversion.hpp"
#include "udm_exception.hpp"

#define UDM_ASSERT_COMPARISON(res) \
	if constexpr(ENABLE_COMPARISON_EXCEPTION) \
	{ \
		if(!res) \
			throw ComparisonError{std::string{"Comparison failure "} + " in " + __FILE__ + ':' + std::to_string(__LINE__) + ':' + __func__}; \
	}

#pragma warning( push )
#pragma warning( disable : 4715 )
namespace udm
{
	static std::string CONTROL_CHARACTERS = "{}[]<>$,:;";
	static std::string WHITESPACE_CHARACTERS = " \t\f\v\n\r";
	static constexpr auto PATH_SEPARATOR = '/';
	bool is_whitespace_character(char c);
	bool is_control_character(char c);
	bool does_key_require_quotes(const std::string_view &key);

	static constexpr auto ENABLE_COMPARISON_EXCEPTION = false;

	struct AsciiException
		: public Exception
	{
		AsciiException(const std::string &msg,uint32_t lineIdx,uint32_t charIdx);
		uint32_t lineIndex = 0;
		uint32_t charIndex = 0;
	};

	struct SyntaxError : public AsciiException {using AsciiException::AsciiException;};
	struct DataError : public AsciiException {using AsciiException::AsciiException;};

	struct Blob
	{
		Blob()=default;
		Blob(const Blob&)=default;
		Blob(Blob&&)=default;
		Blob(std::vector<uint8_t> &&data)
			: data{data}
		{}
		std::vector<uint8_t> data;

		Blob &operator=(Blob &&other);
		Blob &operator=(const Blob &other);

		bool operator==(const Blob &other) const
		{
			auto res = (data == other.data);
			UDM_ASSERT_COMPARISON(res);
			return res;
		}
		bool operator!=(const Blob &other) const {return !operator==(other);}
	};

	struct BlobLz4
	{
		BlobLz4()=default;
		BlobLz4(const BlobLz4&)=default;
		BlobLz4(BlobLz4&&)=default;
		BlobLz4(std::vector<uint8_t> &&compressedData,size_t uncompressedSize)
			: compressedData{compressedData},uncompressedSize{uncompressedSize}
		{}
		size_t uncompressedSize = 0;
		std::vector<uint8_t> compressedData;

		BlobLz4 &operator=(BlobLz4 &&other);
		BlobLz4 &operator=(const BlobLz4 &other);

		bool operator==(const BlobLz4 &other) const
		{
			auto res = (uncompressedSize == other.uncompressedSize && compressedData == other.compressedData);
			UDM_ASSERT_COMPARISON(res);
			return res;
		}
		bool operator!=(const BlobLz4 &other) const {return !operator==(other);}
	};

	struct Utf8String
	{
		Utf8String()=default;
		Utf8String(std::vector<uint8_t> &&data)
			: data{data}
		{}
		Utf8String(const Utf8String &str)
			: data{str.data}
		{}
		std::vector<uint8_t> data;

		Utf8String &operator=(Utf8String &&other);
		Utf8String &operator=(const Utf8String &other);

		bool operator==(const Utf8String &other) const
		{
			auto res = (data == other.data);
			UDM_ASSERT_COMPARISON(res);
			return res;
		}
		bool operator!=(const Utf8String &other) const {return !operator==(other);}
	};

	enum class ArrayType : uint8_t
	{
		Raw = 0,
		Compressed
	};

	enum class BlobResult : uint8_t
	{
		Success = 0,
		DecompressedSizeMismatch,
		InsufficientSize,
		ValueTypeMismatch,
		NotABlobType,
		InvalidProperty
	};

	enum class MergeFlags : uint32_t
	{
		None = 0u,
		OverwriteExisting = 1u
	};

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
	Type ascii_type_to_enum(const std::string_view &type);
	void sanitize_key_name(std::string &key);

	Blob decompress_lz4_blob(const BlobLz4 &data);
	Blob decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize);
	void decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize,void *outData);
	BlobLz4 compress_lz4_blob(const Blob &data);
	BlobLz4 compress_lz4_blob(const void *data,uint64_t size);
	template<class T>
		BlobLz4 compress_lz4_blob(const T &v)
	{
		return compress_lz4_blob(v.data(),v.size() *sizeof(v[0]));
	}
	
	struct Property
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
	struct Header
	{
		Header()=default;
		std::array<char,4> identifier = {HEADER_IDENTIFIER[0],HEADER_IDENTIFIER[1],HEADER_IDENTIFIER[2],HEADER_IDENTIFIER[3]};
		Version version = VERSION;
	};
#pragma pack(pop)

	template<typename T>
		class ArrayIterator
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = T&;
		using difference_type = std::ptrdiff_t;
		using pointer = T*;
		using reference = T&;
	
		ArrayIterator();
		ArrayIterator(udm::Array &a);
		ArrayIterator(udm::Array &a,uint32_t idx);
		ArrayIterator(const ArrayIterator &other);
		ArrayIterator &operator++();
		ArrayIterator operator++(int);
		ArrayIterator operator+(uint32_t n);
		reference operator*();
		pointer operator->();
		bool operator==(const ArrayIterator &other) const;
		bool operator!=(const ArrayIterator &other) const;

		udm::LinkedPropertyWrapper &GetProperty() {return m_curProperty;}
	private:
		udm::LinkedPropertyWrapper m_curProperty;
	};

	struct ElementIteratorWrapper
	{
		ElementIteratorWrapper(LinkedPropertyWrapper &prop);
		ElementIterator begin();
		ElementIterator end();
	private:
		LinkedPropertyWrapper &m_prop;
	};
	
	struct PropertyWrapper
	{
		PropertyWrapper()=default;
		PropertyWrapper(Property &o);
		PropertyWrapper(const PropertyWrapper &other);
		PropertyWrapper(Array &array,uint32_t idx);
		template<typename T>
			void operator=(T &&v) const;
		void operator=(const PropertyWrapper &other);
		void operator=(PropertyWrapper &other);
		void operator=(PropertyWrapper &&other);
		void operator=(const LinkedPropertyWrapper &other);
		void operator=(LinkedPropertyWrapper &other);
		void operator=(Property &other);
		//template<typename T>
		//	operator T() const;
		LinkedPropertyWrapper Add(const std::string_view &path,Type type=Type::Element) const;
		LinkedPropertyWrapper AddArray(const std::string_view &path,std::optional<uint32_t> size={},Type type=Type::Element,ArrayType arrayType=ArrayType::Raw) const;
		LinkedPropertyWrapper AddArray(const std::string_view &path,StructDescription &&strct,std::optional<uint32_t> size={},ArrayType arrayType=ArrayType::Raw) const;
		LinkedPropertyWrapper AddArray(const std::string_view &path,const StructDescription &strct,std::optional<uint32_t> size={},ArrayType arrayType=ArrayType::Raw) const;
		template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path,const StructDescription &strct,const T *data,uint32_t strctItems,ArrayType arrayType=ArrayType::Raw) const;
		template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path,const StructDescription &strct,const std::vector<T> &values,ArrayType arrayType=ArrayType::Raw) const;
		template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path,const std::vector<T> &values,ArrayType arrayType=ArrayType::Raw) const;
		template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path,uint32_t size,const T *data,ArrayType arrayType=ArrayType::Raw) const;
		bool IsArrayItem() const;
		bool IsType(Type type) const;
		Type GetType() const;
		void Merge(const PropertyWrapper &other,MergeFlags mergeFlags=MergeFlags::OverwriteExisting) const;

		Array *GetOwningArray() const;
		BlobResult GetBlobData(void *outBuffer,size_t bufferSize,uint64_t *optOutRequiredSize=nullptr) const;
		BlobResult GetBlobData(void *outBuffer,size_t bufferSize,Type type,uint64_t *optOutRequiredSize=nullptr) const;
		Blob GetBlobData(Type &outType) const;
		template<class T>
			BlobResult GetBlobData(T &v) const;
		template<typename T>
			T &GetValue() const;
		template<typename T>
			T *GetValuePtr() const;
		void *GetValuePtr(Type &outType) const;
		template<typename T>
			T ToValue(const T &defaultValue,bool *optOutIsDefined=nullptr) const;
		template<typename T>
			std::optional<T> ToValue() const;
		template<typename T>
			T operator()(const T &defaultValue) const {return ToValue<T>(defaultValue);}
		template<typename T>
			T operator()(const T &defaultValue,bool &outIsDefined) const {return ToValue<T>(defaultValue,&outIsDefined);}
		template<typename T>
			bool operator()(T &valOut) const
			{
				if constexpr(util::is_specialization<T,std::vector>::value || util::is_specialization<T,std::map>::value || util::is_specialization<T,std::unordered_map>::value)
				{
					bool isDefined;
					valOut = std::move((*this)(const_cast<const T&>(valOut),isDefined));
					return isDefined;
				}
				else if constexpr(std::is_enum_v<std::remove_reference_t<T>>)
				{
					using TEnum = std::remove_reference_t<T>;
					auto *ptr = GetValuePtr<std::string>();
					if(ptr)
					{
						auto e = magic_enum::enum_cast<TEnum>(*ptr);
						if(!e.has_value())
							return false;
						valOut = *e;
						return true;
					}
					bool isDefined;
					valOut = static_cast<TEnum>((*this)(reinterpret_cast<const std::underlying_type_t<TEnum>&>(valOut),isDefined));
					return isDefined;
				}
				else if constexpr(std::is_same_v<std::remove_reference_t<T>,PProperty>)
				{
					if(!prop)
						return false;
					valOut->Assign<false>(*prop);
					return true;
				}
				else if constexpr(std::is_same_v<std::remove_reference_t<T>,Property>)
				{
					if(!prop)
						return false;
					valOut.Assign<false>(*prop);
					return true;
				}
				else
				{
					auto *ptr = GetValuePtr<T>();
					if(ptr)
					{
						valOut = *ptr;
						return true;
					}
					bool isDefined;
					valOut = (*this)(const_cast<const T&>(valOut),isDefined);
					return isDefined;
				}
			}
		
		// For array properties
		uint32_t GetSize() const;
		void Resize(uint32_t size) const;
		template<typename T>
			ArrayIterator<T> begin() const;
		template<typename T>
			ArrayIterator<T> end() const;
		ArrayIterator<LinkedPropertyWrapper> begin() const;
		ArrayIterator<LinkedPropertyWrapper> end() const;
		LinkedPropertyWrapper operator[](uint32_t idx) const;
		LinkedPropertyWrapper operator[](int32_t idx) const;
		LinkedPropertyWrapper operator[](size_t idx) const;
		//

		// For element properties
		ElementIterator begin_el() const;
		ElementIterator end_el() const;
		uint32_t GetChildCount() const;
		//
		
		LinkedPropertyWrapper GetFromPath(const std::string_view &key) const;
		LinkedPropertyWrapper operator[](const std::string_view &key) const;
		LinkedPropertyWrapper operator[](const std::string &key) const;
		LinkedPropertyWrapper operator[](const char *key) const;
		bool operator==(const PropertyWrapper &other) const;
		bool operator!=(const PropertyWrapper &other) const;
		template<typename T>
			bool operator==(const T &other) const;
		template<typename T>
			bool operator!=(const T &other) const {return !operator==(other);}
		Property *operator*() const;
		Property *operator->() const {return operator*();}
		operator bool() const;
		Property *prop = nullptr;
		uint32_t arrayIndex = std::numeric_limits<uint32_t>::max();

		LinkedPropertyWrapper *GetLinked();
		const LinkedPropertyWrapper *GetLinked() const {return const_cast<PropertyWrapper*>(this)->GetLinked();};
	protected:
		bool linked = false;
	};

	struct LinkedPropertyWrapper
		: public PropertyWrapper
	{
		LinkedPropertyWrapper()
			: PropertyWrapper{}
		{
			linked = true;
		}
		LinkedPropertyWrapper(const LinkedPropertyWrapper &other);
		LinkedPropertyWrapper(const PropertyWrapper &other)
			: PropertyWrapper{other}
		{
			linked = true;
		}
		LinkedPropertyWrapper(Property &o)
			: PropertyWrapper{o}
		{
			linked = true;
		}
		LinkedPropertyWrapper(Array &array,uint32_t idx)
			: PropertyWrapper{array,idx}
		{
			linked = true;
		}
		bool operator==(const LinkedPropertyWrapper &other) const;
		bool operator!=(const LinkedPropertyWrapper &other) const;
		using PropertyWrapper::operator==;
		using PropertyWrapper::operator!=;
		template<typename T>
			void operator=(T &&v) const;
		void operator=(PropertyWrapper &&v);
		void operator=(LinkedPropertyWrapper &&v);
		void operator=(const PropertyWrapper &v);
		void operator=(const LinkedPropertyWrapper &v);
		std::string GetPath() const;
		PProperty ClaimOwnership() const;
		ElementIteratorWrapper ElIt();
		std::unique_ptr<LinkedPropertyWrapper> prev = nullptr;
		std::string propName;

		// For internal use only!
		void InitializeProperty(Type type=Type::Element,bool getOnly=false);
		Property *GetProperty(std::vector<uint32_t> *optOutArrayIndices=nullptr) const;
	};

	struct Reference
	{
		Reference()=default;
		Reference(const std::string &path)
			: path{path}
		{}
		Reference(const Reference &other)
			: property{other.property},path{other.path}
		{}
		Reference(Reference &&other)
			: property{other.property},path{std::move(other.path)}
		{}
		Property *property = nullptr;
		std::string path;

		Reference &operator=(Reference &&other);
		Reference &operator=(const Reference &other);

		bool operator==(const Reference &other) const
		{
			auto res = (property == other.property);
			UDM_ASSERT_COMPARISON(res);
			return res;
		}
		bool operator!=(const Reference &other) const {return !operator==(other);}
	private:
		friend Data;
		void InitializeProperty(const LinkedPropertyWrapper &root);
	};

	struct StructDescription
	{
		using SizeType = uint16_t;
		using MemberCountType = uint8_t;
		std::string GetTemplateArgumentList() const;
		SizeType GetDataSizeRequirement() const;
		MemberCountType GetMemberCount() const;

		// TODO: Use these once C++20 is available
		// bool operator==(const Struct&) const=default;
		// bool operator!=(const Struct&) const=default;
		bool operator==(const StructDescription &other) const;
		bool operator!=(const StructDescription &other) const {return !operator==(other);}

		template<typename T1,typename T2,typename ...T>
			static StructDescription Define(std::initializer_list<std::string> names)
		{
			StructDescription strct {};
			strct.DefineTypes<T1,T2,T...>(names);
			return strct;
		}

		template<typename T1,typename T2,typename ...T>
			void DefineTypes(std::initializer_list<std::string> names)
		{
			Clear();
			constexpr auto n = sizeof...(T) +2;
			if(names.size() != n)
				throw InvalidUsageError{"Number of member names has to match number of member types!"};
			types.reserve(n);
			this->names.reserve(n);
			DefineTypes<T1,T2,T...>(names.begin());
		}

		void Clear()
		{
			types.clear();
			names.clear();
		}
		std::vector<Type> types;
		std::vector<String> names;
	private:
		template<typename T1,typename T2,typename ...T>
			void DefineTypes(std::initializer_list<std::string>::iterator it)
		{
			DefineTypes<T1>(it);
			DefineTypes<T2,T...>(it +1);
		}
		template<typename T>
			void DefineTypes(std::initializer_list<std::string>::iterator it)
		{
			types.push_back(type_to_enum<T>());
			names.push_back(*it);
		}
	};

	struct Struct
	{
		static constexpr auto MAX_SIZE = std::numeric_limits<StructDescription::SizeType>::max();
		static constexpr auto MAX_MEMBER_COUNT = std::numeric_limits<StructDescription::MemberCountType>::max();
		Struct()=default;
		Struct(const Struct&)=default;
		Struct(Struct&&)=default;
		Struct(const StructDescription &desc);
		Struct(StructDescription &&desc);
		Struct &operator=(const Struct&)=default;
		Struct &operator=(Struct&&)=default;
		template<class T>
			Struct &operator=(const T &other);
		// TODO: Use these once C++20 is available
		// bool operator==(const Struct&) const=default;
		// bool operator!=(const Struct&) const=default;
		bool operator==(const Struct &other) const;
		bool operator!=(const Struct &other) const {return !operator==(other);}

		StructDescription &operator*() {return description;}
		const StructDescription &operator*() const {return const_cast<Struct*>(this)->operator*();}
		StructDescription *operator->() {return &description;}
		const StructDescription *operator->() const {return const_cast<Struct*>(this)->operator->();}

		void Clear();
		void UpdateData();
		void SetDescription(const StructDescription &desc);
		void SetDescription(StructDescription &&desc);
		StructDescription description;
		std::vector<uint8_t> data;
	};

	struct Element
	{
		void AddChild(std::string &&key,const PProperty &o);
		void AddChild(const std::string &key,const PProperty &o);
		std::unordered_map<std::string,PProperty> children;
		PropertyWrapper fromProperty {};
		PropertyWrapper parentProperty {};
		
		LinkedPropertyWrapper operator[](const std::string &key) {return fromProperty[key];}
		LinkedPropertyWrapper operator[](const char *key) {return operator[](std::string{key});}

		LinkedPropertyWrapper Add(const std::string_view &path,Type type=Type::Element);
		LinkedPropertyWrapper AddArray(const std::string_view &path,std::optional<uint32_t> size={},Type type=Type::Element,ArrayType arrayType=ArrayType::Raw);
		void ToAscii(AsciiSaveFlags flags,std::stringstream &ss,const std::optional<std::string> &prefix={}) const;

		void Merge(const Element &other,MergeFlags mergeFlags=MergeFlags::OverwriteExisting);

		bool operator==(const Element &other) const;
		bool operator!=(const Element &other) const {return !operator==(other);}
		Element &operator=(Element &&other);
		Element &operator=(const Element &other);

		explicit operator PropertyWrapper&() {return fromProperty;}

		ElementIterator begin();
		ElementIterator end();
	private:
		friend PropertyWrapper;
		template<typename T>
			void SetValue(Element &child,T &&v);
	};

	struct ElementIteratorPair
	{
		ElementIteratorPair(std::unordered_map<std::string,PProperty>::iterator &it);
		ElementIteratorPair();
		bool operator==(const ElementIteratorPair &other) const;
		bool operator!=(const ElementIteratorPair &other) const;
		std::string_view key;
		LinkedPropertyWrapper property;
	};

	class ElementIterator
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = ElementIteratorPair&;
		using difference_type = std::ptrdiff_t;
		using pointer = ElementIteratorPair*;
		using reference = ElementIteratorPair&;
	
		ElementIterator();
		ElementIterator(udm::Element &e);
		ElementIterator(udm::Element &e,std::unordered_map<std::string,PProperty>::iterator it);
		ElementIterator(const ElementIterator &other);
		ElementIterator &operator++();
		ElementIterator operator++(int);
		reference operator*();
		pointer operator->();
		bool operator==(const ElementIterator &other) const;
		bool operator!=(const ElementIterator &other) const;
	private:
		std::unordered_map<std::string,PProperty>::iterator m_iterator {};
		ElementIteratorPair m_pair;
	};

	struct Array
	{
		virtual ~Array();
		PropertyWrapper fromProperty {};

		bool operator==(const Array &other) const;
		bool operator!=(const Array &other) const {return !operator==(other);}

		void Merge(const Array &other,MergeFlags mergeFlags=MergeFlags::OverwriteExisting);

		virtual Array &operator=(Array &&other);
		virtual Array &operator=(const Array &other);
		
		virtual void SetValueType(Type valueType);
		virtual ArrayType GetArrayType() const {return ArrayType::Raw;}
		bool IsValueType(Type pvalueType) const {return pvalueType == m_valueType;}
		Type GetValueType() const {return m_valueType;}
		uint32_t GetSize() const {return m_size;}
		uint32_t GetValueSize() const;
		virtual void *GetValues() {return GetValuePtr();}
		const void *GetValues() const {return const_cast<Array*>(this)->GetValues();}
		void Resize(uint32_t newSize);
		void *GetValuePtr(uint32_t idx);
		template<typename T>
			T *GetValuePtr(uint32_t idx);
		template<typename T>
			const T *GetValuePtr(uint32_t idx) const {return const_cast<Array*>(this)->GetValuePtr<T>(idx);}
		PropertyWrapper operator[](uint32_t idx);
		const PropertyWrapper operator[](uint32_t idx) const {return const_cast<Array*>(this)->operator[](idx);}
		template<typename T>
			T &GetValue(uint32_t idx);
		template<typename T>
			const T &GetValue(uint32_t idx) const {return const_cast<Array*>(this)->GetValue<T>(idx);}
		template<typename T>
			void SetValue(uint32_t idx,T &&value);
		template<typename T>
			void InsertValue(uint32_t idx,T &&value);
		void RemoveValue(uint32_t idx);
		// The caller is responsible to ensure that the type of value matches the value type of the array!
		void SetValue(uint32_t idx,const void *value);
		void InsertValue(uint32_t idx,void *value);

		bool IsEmpty() const {return m_size == 0;}
		template<typename T>
			T *GetFront() {return !IsEmpty() ? GetValuePtr<T>(0u) : nullptr;}
		template<typename T>
			const T *GetFront() const {return const_cast<Array*>(this)->GetFront<T>();}
		template<typename T>
			T *GetBack() {return !IsEmpty() ? GetValuePtr<T>(m_size -1) : nullptr;}
		template<typename T>
			const T *GetBack() const {return const_cast<Array*>(this)->GetBack<T>();}

		template<typename T>
			ArrayIterator<T> begin()
		{
			return ArrayIterator<T>{*this};
		}
		template<typename T>
			ArrayIterator<T> end()
		{
			return ArrayIterator<T>{*this,GetSize()};
		}
		ArrayIterator<LinkedPropertyWrapper> begin() {return begin<LinkedPropertyWrapper>();}
		ArrayIterator<LinkedPropertyWrapper> end() {return end<LinkedPropertyWrapper>();}

		uint64_t GetByteSize() const;
		const StructDescription *GetStructuredDataInfo() const {return const_cast<Array*>(this)->GetStructuredDataInfo();}
		virtual StructDescription *GetStructuredDataInfo();

		static constexpr bool IsValueTypeSupported(Type type)
		{
			return true;
		}
	protected:
		friend Property;
		friend PropertyWrapper;
		virtual void Clear();
		using Range = std::tuple<uint32_t,uint32_t,uint32_t>;
		void Resize(uint32_t newSize,Range r0,Range r1,bool defaultInitializeNewValues);

		void *GetValuePtr();
		const void *GetValuePtr() const {return const_cast<Array*>(this)->GetValuePtr();}
		void *GetHeaderPtr();
		const void *GetHeaderPtr() const {return const_cast<Array*>(this)->GetHeaderPtr();}
		uint64_t GetHeaderSize() const;
		void ReleaseValues();
		uint8_t *AllocateData(uint64_t size) const;

		void *m_values = nullptr;
		uint32_t m_size = 0;
		Type m_valueType = Type::Nil;
	};

	struct ArrayLz4
		: public Array
	{
		enum class State : uint8_t
		{
			Compressed = 0,
			Uncompressed
		};
		
		ArrayLz4()=default;
		virtual ArrayLz4 &operator=(Array &&other) override;
		virtual ArrayLz4 &operator=(const Array &other) override;
		ArrayLz4 &operator=(ArrayLz4 &&other);
		ArrayLz4 &operator=(const ArrayLz4 &other);
		const BlobLz4 &GetCompressedBlob() const {return const_cast<ArrayLz4*>(this)->GetCompressedBlob();}
		BlobLz4 &GetCompressedBlob();
		virtual void *GetValues() override;
		virtual void SetValueType(Type valueType) override;
		virtual ArrayType GetArrayType() const {return ArrayType::Compressed;}
		void ClearUncompressedMemory();
		using Array::GetStructuredDataInfo;

		static constexpr bool IsValueTypeSupported(Type type)
		{
			return is_numeric_type(type) || is_generic_type(type) || type == Type::Struct || type == Type::Element || type == Type::String;
		}
	private:
		friend Property;
		friend PropertyWrapper;
		friend AsciiReader;
		virtual StructDescription *GetStructuredDataInfo() override;
		void InitializeSize(uint32_t size);
		void Decompress();
		void Compress();
		virtual void Clear() override;
		std::unique_ptr<StructDescription> m_structuredDataInfo = nullptr;
		State m_state = State::Uncompressed;
		BlobLz4 m_compressedBlob {};
	};

	struct AssetData
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

	enum class FormatType : uint8_t
	{
		Binary = 0,
		Ascii
	};

	enum class AsciiSaveFlags : uint32_t
	{
		None = 0u,
		IncludeHeader = 1u,
		DontCompressLz4Arrays = IncludeHeader<<1u
	};

	struct IFile
	{
		template<typename T>
			T Read()
		{
			T v;
			Read(&v,sizeof(T));
			return v;
		}
		template<typename T>
			size_t Write(const T &v)
		{
			return Write(&v,sizeof(T));
		}
		enum class Whence : uint8_t
		{
			Set = 0,
			Cur,
			End
		};
		virtual ~IFile()=default;
		virtual size_t Read(void *data,size_t size)=0;
		virtual size_t Write(const void *data,size_t size)=0;
		virtual size_t Tell()=0;
		virtual void Seek(size_t offset,Whence whence=Whence::Set)=0;
		virtual int32_t ReadChar()=0;
		size_t GetSize()
		{
			auto pos = Tell();
			Seek(0,Whence::End);
			auto size = Tell();
			Seek(pos);
			return size;
		}

		int32_t WriteString(const std::string &str);
	};

	struct MemoryFile
		: public udm::IFile
	{
		MemoryFile(uint8_t *data,size_t dataSize);
		void *GetData() {return m_data;}
		const void *GetData() const {return const_cast<MemoryFile*>(this)->GetData();}
		size_t GetDataSize() const {return m_dataSize;}
		virtual size_t Read(void *data,size_t size) override;
		virtual size_t Write(const void *data,size_t size) override;
		virtual size_t Tell() override;
		virtual void Seek(size_t offset,Whence whence=Whence::Set) override;
		virtual int32_t ReadChar() override;
		char *GetMemoryDataPtr();

		template<typename T>
			T &GetValue() {return *reinterpret_cast<T*>(GetMemoryDataPtr());}
		template<typename T>
			const T &GetValue() const {return const_cast<MemoryFile*>(this)->GetValue<T>();}

		template<typename T>
			T &GetValueAndAdvance()
		{
			auto &val = GetValue<T>();
			Seek(sizeof(T),Whence::Cur);
			return val;
		}
		template<typename T>
			const T &GetValueAndAdvance() const {return const_cast<MemoryFile*>(this)->GetValueAndAdvance<T>();}
	protected:
		uint8_t *m_data = nullptr;
		size_t m_dataSize = 0;
		size_t m_pos = 0;
	};

	struct VectorFile
		: public MemoryFile
	{
		VectorFile();
		VectorFile(size_t size);
		const std::vector<uint8_t> &GetVector() const;
		void Resize(size_t size);
	private:
		std::vector<uint8_t> m_data;
	};

	struct VFilePtr
		: public IFile
	{
		VFilePtr()=default;
		VFilePtr(const ::VFilePtr &f);
		virtual ~VFilePtr() override=default;
		virtual size_t Read(void *data,size_t size) override;
		virtual size_t Write(const void *data,size_t size) override;
		virtual size_t Tell() override;
		virtual void Seek(size_t offset,Whence whence=Whence::Set) override;
		virtual int32_t ReadChar() override;
	private:
		::VFilePtr m_file;
	};

	class Data
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
};
REGISTER_BASIC_BITWISE_OPERATORS(udm::AsciiSaveFlags)
REGISTER_BASIC_BITWISE_OPERATORS(udm::MergeFlags)

template<bool ENABLE_EXCEPTIONS,typename T>
	bool udm::Property::Assign(T &&v)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if constexpr(util::is_specialization<TBase,std::vector>::value)
	{
		using TValueType = TBase::value_type;
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

		if(size_of_base_type(valueType) != sizeof(TBase::value_type))
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
		if constexpr(is_convertible<decltype(tag)::type,T>())
			return convert<decltype(tag)::type,T>(const_cast<udm::Property*>(this)->GetValue<decltype(tag)::type>());
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
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,const StructDescription &strct,const T *data,uint32_t strctItems,ArrayType arrayType) const
{
	auto prop = AddArray(path,strct,strctItems,arrayType);
	auto &a = prop.GetValue<Array>();
	auto sz = a.GetValueSize() *a.GetSize();
	auto *ptr = a.GetValues();
	memcpy(ptr,data,sz);
	return prop;
}

template<typename T>
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,const StructDescription &strct,const std::vector<T> &values,ArrayType arrayType) const
{
	auto prop = AddArray(path,strct,values.size(),arrayType);
	auto &a = prop.GetValue<Array>();
	auto sz = a.GetValueSize() *a.GetSize();
	auto szValues = util::size_of_container(values);
	if(szValues != sz)
		throw InvalidUsageError{"Size of values does not match expected size of defined struct!"};
	auto *ptr = a.GetValues();
	memcpy(ptr,values.data(),szValues);
	return prop;
}

template<typename T>
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,const std::vector<T> &values,ArrayType arrayType) const
{
	return AddArray<T>(path,values.size(),values.data(),arrayType);
}

template<typename T>
	udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,uint32_t size,const T *data,ArrayType arrayType) const
{
	constexpr auto valueType = type_to_enum<T>();
	auto prop = AddArray(path,size,valueType,arrayType);
	auto &a = prop.GetValue<Array>();
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
	if constexpr(is_trivial_type(type_to_enum_s<T::value_type>()))
	{
		if(is_array_type(this->type))
		{
			auto &a = GetValue<Array>();
			if(a.GetValueType() == type_to_enum<T::value_type>())
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
	if(IsArrayItem())
		return BlobResult::NotABlobType;
	return (*this)->GetBlobData(v);
}
template<typename T>
	T &udm::PropertyWrapper::GetValue() const
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValue<T>();
		if(a.IsValueType(type_to_enum<T>()) == false)
			throw LogicError{"Type mismatch, requested type is " +std::string{magic_enum::enum_name(type_to_enum<T>())} +", but actual type is " +std::string{magic_enum::enum_name(a.GetValueType())} +"!"};
		return static_cast<T*>(a.GetValues())[arrayIndex];
	}
	return (*this)->GetValue<T>();
}

template<typename T>
	T *udm::PropertyWrapper::GetValuePtr() const
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValuePtr<T>();
		if(a.IsValueType(type_to_enum<T>()) == false)
			return nullptr;
		return &static_cast<T*>(a.GetValues())[arrayIndex];
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
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->ToValue<T>();
		auto vs = [&](auto tag) -> std::optional<T> {
			if constexpr(is_convertible<decltype(tag)::type,T>())
				return std::optional<T>{convert<decltype(tag)::type,T>(const_cast<udm::PropertyWrapper*>(this)->GetValue<decltype(tag)::type>())};
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
