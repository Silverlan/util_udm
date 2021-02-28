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
#include <unordered_map>
#include <mathutil/uvec.h>
#include <mathutil/transform.hpp>
#include <sharedutils/util.h>
#include <sharedutils/util_string.h>
#include <sharedutils/magic_enum.hpp>
#include <fsys/filesystem.h>

namespace udm
{
	using DataValue = void*;
	using String = std::string;
	using Int8 = int8_t;
	using UInt8 = uint8_t;
	using Int16 = int16_t;
	using UInt16 = uint16_t;
	using Int32 = int32_t;
	using UInt32 = uint32_t;
	using Int64 = int64_t;
	using UInt64 = uint64_t;
	using Enum = int32_t;

	using Float = float;
	using Double = double;
	using Boolean = bool;

	using Vector2 = ::Vector2;
	using Vector3 = ::Vector3;
	using Vector4 = ::Vector4;
	using Quaternion = Quat;
	using EulerAngles = ::EulerAngles;
	using Srgba = std::array<uint8_t,4>;
	using HdrColor = std::array<uint16_t,3>;
	using Transform = umath::Transform;
	using ScaledTransform = umath::ScaledTransform;
	using Mat4 = Mat4;
	using Mat3x4 = Mat3x4;

	using Nil = std::monostate;
	struct Blob
	{
		Blob()=default;
		Blob(std::vector<uint8_t> &&data)
			: data{data}
		{}
		std::vector<uint8_t> data;
	};

	struct BlobLz4
	{
		BlobLz4()=default;
		BlobLz4(std::vector<uint8_t> &&compressedData,size_t uncompressedSize)
			: compressedData{compressedData},uncompressedSize{uncompressedSize}
		{}
		size_t uncompressedSize = 0;
		std::vector<uint8_t> compressedData;
	};

	struct Utf8String
	{
		Utf8String()=default;
		Utf8String(std::vector<uint8_t> &&data)
			: data{data}
		{}
		std::vector<uint8_t> data;
	};
	enum class Type : uint8_t
	{
		Nil = 0,
		String,
		Utf8String,

		Int8,
		UInt8,
		Int16,
		UInt16,
		Int32,
		UInt32,
		Int64,
		UInt64,

		Float,
		Double,
		Boolean,

		Vector2,
		Vector3,
		Vector4,
		Quaternion,
		EulerAngles,
		Srgba,
		HdrColor,
		Transform,
		ScaledTransform,
		Mat4,
		Mat3x4,

		Blob,
		BlobLz4,

		Element,
		Array,

		Count
	};
	static std::array<Type,6> NON_TRIVIAL_TYPES = {Type::String,Type::Utf8String,Type::Blob,Type::BlobLz4,Type::Element,Type::Array};

	enum class BlobResult : uint8_t
	{
		Success = 0,
		DecompressedSizeMismatch,
		InsufficientSize,
		ValueTypeMismatch,
		NotABlobType
	};

	constexpr bool is_numeric_type(Type t)
	{
	  switch (t){
		case Type::Int8:
		case Type::UInt8:
		case Type::Int16:
		case Type::UInt16:
		case Type::Int32:
		case Type::UInt32:
		case Type::Int64:
		case Type::UInt64:
		case Type::Float:
		case Type::Double:
		case Type::Boolean:
			return true;
	  }
	  return false;
	}

	constexpr bool is_generic_type(Type t)
	{
	  switch (t){
		case Type::Vector2:
		case Type::Vector3:
		case Type::Vector4:
		case Type::Quaternion:
		case Type::EulerAngles:
		case Type::Srgba:
		case Type::HdrColor:
		case Type::Transform:
		case Type::ScaledTransform:
		case Type::Mat4:
		case Type::Mat3x4:
		case Type::Nil:
			return true;
	  }
	  return false;
	}

	constexpr bool is_non_trivial_type(Type t)
	{
	  switch (t){
		case Type::String:
		case Type::Utf8String:
		case Type::Blob:
		case Type::BlobLz4:
		case Type::Element:
		case Type::Array:
			return true;
	  }
	  static_assert(NON_TRIVIAL_TYPES.size() == 6,"Update this list when new non-trivial types have been added!");
	  return false;
	}

	constexpr bool is_trivial_type(Type t) {return !is_non_trivial_type(t);}

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
		case Type::Vector3:
			return "vec3";
		case Type::Vector4:
			return "vec4";
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
		case Type::Element:
			return "element";
		}
		static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	}
	Type ascii_type_to_enum(const char *type);
	void sanitize_key_name(std::string &key);

	template<class T>struct tag_t{using type=T;};
	template<class T>constexpr tag_t<T> tag={};
	constexpr std::variant<tag_t<Int8>,tag_t<UInt8>,tag_t<Int16>,tag_t<UInt16>,tag_t<Int32>,tag_t<UInt32>,tag_t<Int64>,tag_t<UInt64>,tag_t<Float>,tag_t<Double>,tag_t<Boolean>> get_numeric_tag(Type e)
	{
		switch(e)
		{
			case Type::Int8: return tag<Int8>;
			case Type::UInt8: return tag<UInt8>;
			case Type::Int16: return tag<Int16>;
			case Type::UInt16: return tag<UInt16>;
			case Type::Int32: return tag<Int32>;
			case Type::UInt32: return tag<UInt32>;
			case Type::Int64: return tag<Int64>;
			case Type::UInt64: return tag<UInt64>;
			case Type::Float: return tag<Float>;
			case Type::Double: return tag<Double>;
			case Type::Boolean: return tag<Boolean>;
		}
	}

	constexpr std::variant<tag_t<Vector2>,tag_t<Vector3>,tag_t<Vector4>,tag_t<Quaternion>,tag_t<EulerAngles>,tag_t<Srgba>,tag_t<HdrColor>,tag_t<Transform>,tag_t<ScaledTransform>,tag_t<Mat4>,tag_t<Mat3x4>,tag_t<Nil>> get_generic_tag(Type e)
	{
		switch(e)
		{
			case Type::Vector2: return tag<Vector2>;
			case Type::Vector3: return tag<Vector3>;
			case Type::Vector4: return tag<Vector4>;
			case Type::Quaternion: return tag<Quaternion>;
			case Type::EulerAngles: return tag<EulerAngles>;
			case Type::Srgba: return tag<Srgba>;
			case Type::HdrColor: return tag<HdrColor>;
			case Type::Transform: return tag<Transform>;
			case Type::ScaledTransform: return tag<ScaledTransform>;
			case Type::Mat4: return tag<Mat4>;
			case Type::Mat3x4: return tag<Mat3x4>;
			case Type::Nil: return tag<Nil>;
		}
	}

	struct Element;
	struct Array;
	constexpr std::variant<tag_t<String>,tag_t<Utf8String>,tag_t<Blob>,tag_t<BlobLz4>,tag_t<Element>,tag_t<Array>> get_non_trivial_tag(Type e)
	{
		switch(e)
		{
			case Type::String: return tag<String>;
			case Type::Utf8String: return tag<Utf8String>;
			case Type::Blob: return tag<Blob>;
			case Type::BlobLz4: return tag<BlobLz4>;
			case Type::Element: return tag<Element>;
			case Type::Array: return tag<Array>;
		}
		static_assert(NON_TRIVIAL_TYPES.size() == 6,"Update this list when new non-trivial types have been added!");
	}

	Blob decompress_lz4_blob(const BlobLz4 &data);
	Blob decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize);
	BlobLz4 compress_lz4_blob(const Blob &data);
	BlobLz4 compress_lz4_blob(const void *data,uint64_t size);
	template<class T>
		BlobLz4 compress_lz4_blob(const T &v)
	{
		return compress_lz4_blob(v.data(),v.size() *sizeof(v[0]));
	}
	constexpr size_t size_of(Type t);
	constexpr size_t size_of_base_type(Type t);
	template<typename T>
		constexpr Type type_to_enum();
	template<typename T>
		constexpr Type array_value_type_to_enum();
	template<typename TFrom,typename TTo>
		constexpr bool is_convertible();
	template<typename TTo>
		constexpr bool is_convertible_from(Type tFrom);
	template<typename TFrom>
		constexpr bool is_convertible(Type tTo);
	constexpr bool is_convertible(Type tFrom,Type tTo);
	
	class LinkedPropertyWrapper;
	struct Array;
	struct Property;
	using PProperty = std::shared_ptr<Property>;
	using WPProperty = std::weak_ptr<Property>;
	struct Property
	{
		template<typename T>
			static PProperty Create(T &&value);
		template<typename T>
			static PProperty Create();
		static PProperty Create() {return Create<void>();}
		static PProperty Create(Type type);
		Property()=default;
		Property(const Property &other)=delete;
		Property(Property &&other);
		~Property();
		bool IsType(Type ptype) const {return ptype == type;}
		Property &operator=(const Property &other)=delete;
		Property &operator=(Property &&other)=delete;
		template<typename T>
			void operator=(T &&v);

		Type type = Type::Nil;
		DataValue value = nullptr;

		LinkedPropertyWrapper operator[](const std::string &key);
		LinkedPropertyWrapper operator[](const char *key);

		BlobResult GetBlobData(void *outBuffer,size_t bufferSize,uint64_t *optOutRequiredSize=nullptr) const;
		BlobResult GetBlobData(void *outBuffer,size_t bufferSize,Type type,uint64_t *optOutRequiredSize=nullptr) const;
		Blob GetBlobData(Type &outType) const;
		template<class T>
			BlobResult GetBlobData(T &v) const
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
			if constexpr(is_trivial_type(type_to_enum<T::value_type>()))
			{
				if(IsType(Type::Array))
				{
					auto &a = GetValue<Array>();
					if(a.valueType == type_to_enum<T::value_type>())
					{
						v.resize(a.GetSize());
						memcpy(v.data(),a.values,v.size() *sizeof(v[0]));
						return BlobResult::Success;
					}
				}
			}
			return BlobResult::NotABlobType;
		}
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
		
		bool Read(const VFilePtr &f,std::string &outErr);
		bool Read(Type type,const VFilePtr &f,std::string &outErr);
		void Write(VFilePtrReal &f) const;

		void ToAscii(std::stringstream &ss,const std::string &propName,const std::string &prefix="");
		
		static void ToAscii(std::stringstream &ss,const std::string &propName,Type type,const DataValue value,const std::string &prefix="");
		bool Read(const VFilePtr &f,Blob &outBlob,std::string &outErr);
		bool Read(const VFilePtr &f,BlobLz4 &outBlob,std::string &outErr);
		bool Read(const VFilePtr &f,Utf8String &outStr,std::string &outErr);
		bool Read(const VFilePtr &f,Element &outEl,std::string &outErr);
		bool Read(const VFilePtr &f,Array &outArray,std::string &outErr);
		bool Read(const VFilePtr &f,String &outStr,std::string &outErr);
		static void Write(VFilePtrReal &f,const Blob &blob);
		static void Write(VFilePtrReal &f,const BlobLz4 &blob);
		static void Write(VFilePtrReal &f,const Utf8String &str);
		static void Write(VFilePtrReal &f,const Element &el);
		static void Write(VFilePtrReal &f,const Array &a);
		static void Write(VFilePtrReal &f,const String &str);
		
		static std::string ToAsciiValue(const Nil &nil,const std::string &prefix="");
		static std::string ToAsciiValue(const Blob &blob,const std::string &prefix="");
		static std::string ToAsciiValue(const BlobLz4 &blob,const std::string &prefix="");
		static std::string ToAsciiValue(const Utf8String &utf8,const std::string &prefix="");
		static std::string ToAsciiValue(const Element &el,const std::string &prefix="");
		static std::string ToAsciiValue(const Array &a,const std::string &prefix="");
		static std::string ToAsciiValue(const String &str,const std::string &prefix="");
		
		static std::string ToAsciiValue(const Vector2 &v,const std::string &prefix="");
		static std::string ToAsciiValue(const Vector3 &v,const std::string &prefix="");
		static std::string ToAsciiValue(const Vector4 &v,const std::string &prefix="");
		static std::string ToAsciiValue(const Quaternion &q,const std::string &prefix="");
		static std::string ToAsciiValue(const EulerAngles &a,const std::string &prefix="");
		static std::string ToAsciiValue(const Srgba &srgb,const std::string &prefix="");
		static std::string ToAsciiValue(const HdrColor &col,const std::string &prefix="");
		static std::string ToAsciiValue(const Transform &t,const std::string &prefix="");
		static std::string ToAsciiValue(const ScaledTransform &t,const std::string &prefix="");
		static std::string ToAsciiValue(const Mat4 &m,const std::string &prefix="");
		static std::string ToAsciiValue(const Mat3x4 &m,const std::string &prefix="");

		static constexpr uint8_t EXTENDED_STRING_IDENTIFIER = std::numeric_limits<uint8_t>::max();
		static BlobResult GetBlobData(const Blob &blob,void *outBuffer,size_t bufferSize);
		static BlobResult GetBlobData(const BlobLz4 &blob,void *outBuffer,size_t bufferSize);
		static Blob GetBlobData(const BlobLz4 &blob);
	private:
		template<typename T>
			static void NumericTypeToString(T value,std::stringstream &ss)
		{
			if constexpr(std::is_same_v<T,Int8> || std::is_same_v<T,UInt8>)
				ss<<+value;
			else
				ss<<value;
		}
		void Initialize();
		void Clear();
		template<typename T>
			T &GetValue(Type type);
	};

	using Version = uint32_t;
	static constexpr Version VERSION = 1;
	static constexpr auto *HEADER_IDENTIFIER = "UDMB";
#pragma pack(push,1)
	struct Header
	{
		Header()=default;
		std::array<char,4> identifier = {HEADER_IDENTIFIER[0],HEADER_IDENTIFIER[1],HEADER_IDENTIFIER[2],HEADER_IDENTIFIER[3]};
		Version version = VERSION;
	};
#pragma pack(pop)

	struct LinkedPropertyWrapper;
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
		reference operator*();
		pointer operator->();
		bool operator==(const ArrayIterator &other) const;
		bool operator!=(const ArrayIterator &other) const;
	private:
		udm::LinkedPropertyWrapper m_curProperty;
	};

	struct PropertyWrapper
	{
		PropertyWrapper()=default;
		PropertyWrapper(Property &o);
		PropertyWrapper(const PropertyWrapper &other);
		PropertyWrapper(Array &array,uint32_t idx);
		template<typename T>
			void operator=(T &&v);
		void operator=(const PropertyWrapper &other);
		void operator=(PropertyWrapper &other);
		void operator=(PropertyWrapper &&other);
		void operator=(const LinkedPropertyWrapper &other);
		void operator=(LinkedPropertyWrapper &other);
		void operator=(Property &other);
		//template<typename T>
		//	operator T() const;
		LinkedPropertyWrapper Add(const std::string_view &path,Type type=Type::Element);
		LinkedPropertyWrapper AddArray(const std::string_view &path,std::optional<uint32_t> size={},Type type=Type::Element);
		bool IsArrayItem() const;

		Array *GetOwningArray();
		const Array *GetOwningArray() const {return const_cast<PropertyWrapper*>(this)->GetOwningArray();}
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
		template<typename T>
			T operator()(const T &defaultValue) const {return ToValue<T>(defaultValue);}
		
		// For array properties
		uint32_t GetSize() const;
		void Resize(uint32_t size);
		template<typename T>
			ArrayIterator<T> begin();
		template<typename T>
			ArrayIterator<T> end();
		ArrayIterator<Element> begin();
		ArrayIterator<Element> end();
		//

		LinkedPropertyWrapper operator[](const std::string &key) const;
		LinkedPropertyWrapper operator[](const char *key) const;
		LinkedPropertyWrapper operator[](uint32_t idx) const;
		LinkedPropertyWrapper operator[](int32_t idx) const;
		LinkedPropertyWrapper operator[](size_t idx) const;
		bool operator==(const PropertyWrapper &other) const;
		bool operator!=(const PropertyWrapper &other) const;
		Property *operator*();
		const Property *operator*() const {return const_cast<PropertyWrapper*>(this)->operator*();}
		Property *operator->() {return operator*();}
		const Property *operator->() const {return const_cast<PropertyWrapper*>(this)->operator->();}
		operator bool() const;
		Property *prop = nullptr;
		uint32_t arrayIndex = std::numeric_limits<uint32_t>::max();
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
		template<typename T>
			void operator=(T &&v);
		std::unique_ptr<LinkedPropertyWrapper> prev = nullptr;
		std::string propName;

		// For internal use only!
		void InitializeProperty(Type type=Type::Element);
	};

	struct Element
	{
		void AddChild(const std::string &key,const PProperty &o);
		std::unordered_map<std::string,PProperty> children;
		PropertyWrapper fromProperty {};
		PropertyWrapper parentProperty {};
		
		LinkedPropertyWrapper operator[](const std::string &key) {return fromProperty[key];}
		LinkedPropertyWrapper operator[](const char *key) {return operator[](std::string{key});}

		LinkedPropertyWrapper Add(const std::string_view &path,Type type=Type::Element);
		LinkedPropertyWrapper AddArray(const std::string_view &path,std::optional<uint32_t> size={},Type type=Type::Element);
		void ToAscii(std::stringstream &ss,const std::optional<std::string> &prefix={}) const;
	private:
		friend PropertyWrapper;
		template<typename T>
			void SetValue(Element &child,T &&v);
	};

	struct Array;
	struct Array
	{
		~Array();
		Type valueType = Type::Nil;
		uint32_t size = 0;
		void *values = nullptr;
		PropertyWrapper fromProperty {};
		
		void SetValueType(Type valueType);
		bool IsValueType(Type pvalueType) const {return pvalueType == valueType;}
		uint32_t GetSize() const {return size;}
		void Resize(uint32_t newSize);
		PropertyWrapper operator[](uint32_t idx);
		const PropertyWrapper operator[](uint32_t idx) const {return const_cast<Array*>(this)->operator[](idx);}
		template<typename T>
			T &GetValue(uint32_t idx);
		template<typename T>
			const T &GetValue(uint32_t idx) const {return const_cast<Array*>(this)->GetValue<T>(idx);}

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
		ArrayIterator<Element> begin() {return begin<Element>();}
		ArrayIterator<Element> end() {return end<Element>();}
	private:
		friend Property;
		friend PropertyWrapper;
		void Clear();
		template<typename T>
			void SetValue(uint32_t idx,T &&v);
	};

	struct AssetData
		: public LinkedPropertyWrapper
	{
		std::string GetAssetType() const;
		Version GetAssetVersion() const;
		void SetAssetType(const std::string &assetType);
		void SetAssetVersion(Version version);

		LinkedPropertyWrapper GetData() const;
		LinkedPropertyWrapper operator*() const {return GetData();}
		LinkedPropertyWrapper operator->() const {return GetData();}
	};

	class Data
	{
	public:
		static constexpr auto KEY_ASSET_TYPE = "assetType";
		static constexpr auto KEY_ASSET_VERSION = "assetVersion";
		static constexpr auto KEY_ASSET_DATA = "assetData";
		static std::shared_ptr<Data> Load(const std::string &fileName,std::string &outErr);
		static std::shared_ptr<Data> Load(const VFilePtr &f,std::string &outErr);
		static std::shared_ptr<Data> Open(const std::string &fileName,std::string &outErr);
		static std::shared_ptr<Data> Open(const VFilePtr &f,std::string &outErr);
		static std::shared_ptr<Data> Create(const std::string &assetType,Version assetVersion,std::string &outErr);
		static std::shared_ptr<Data> Create(std::string &outErr);

		PProperty LoadProperty(const std::string_view &path,std::string &outErr) const;

		bool Save(const std::string &fileName,std::string &outErr) const;
		bool Save(VFilePtrReal &f,std::string &outErr) const;
		bool SaveAscii(const std::string &fileName,std::string &outErr) const;
		bool SaveAscii(VFilePtrReal &f,std::string &outErr) const;
		Element &GetRootElement() {return *static_cast<Element*>(m_rootProperty->value);}
		const Element &GetRootElement() const {return const_cast<Data*>(this)->GetRootElement();}
		AssetData GetAssetData() const;
		
		LinkedPropertyWrapper operator[](const std::string &key) const;
		Element *operator->();
		const Element *operator->() const;
		Element &operator*();
		const Element &operator*() const;

		std::string GetAssetType() const;
		Version GetAssetVersion() const;
		void SetAssetType(const std::string &assetType);
		void SetAssetVersion(Version version);

		void ToAscii(std::stringstream &ss) const;

		const Header &GetHeader() const {return m_header;}

		static std::string ReadKey(const VFilePtr &f);
		static void WriteKey(VFilePtrReal &f,const std::string &key);
	private:
		bool ValidateHeaderProperties(std::string &outErr);
		static void SkipProperty(VFilePtr &f,Type type);
		PProperty LoadProperty(Type type,const std::string_view &path,std::string &outErr) const;
		static PProperty ReadProperty(const VFilePtr &f,std::string &outErr);
		static void WriteProperty(VFilePtrReal &f,const Property &o);
		Data()=default;
		Header m_header;
		VFilePtr m_file = nullptr;
		PProperty m_rootProperty = nullptr;
	};
};

constexpr size_t udm::size_of(Type t)
{
	if(is_numeric_type(t))
	{
		auto tag = get_numeric_tag(t);
		return std::visit([&](auto tag){return sizeof(decltype(tag)::type);},tag);
	}

	if(is_generic_type(t))
	{
		auto tag = get_generic_tag(t);
		return std::visit([&](auto tag){
			if constexpr(std::is_same_v<decltype(tag)::type,std::monostate>)
				return static_cast<uint64_t>(0);
			return sizeof(decltype(tag)::type);
		},tag);
	}
	throw std::logic_error{std::string{"UDM type "} +std::string{magic_enum::enum_name(t)} +" has non-constant size!"};
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	return 0;
}

constexpr size_t udm::size_of_base_type(Type t)
{
	if(is_non_trivial_type(t))
	{
		auto tag = get_non_trivial_tag(t);
		return std::visit([&](auto tag){return sizeof(decltype(tag)::type);},tag);
	}
	return size_of(t);
}

template<typename T>
	constexpr udm::Type udm::array_value_type_to_enum()
{
	static_assert(util::is_specialization<T,std::vector>::value);
	return udm::type_to_enum<T::value_type>();
}

template<typename T>
	constexpr udm::Type udm::type_to_enum()
{
	if constexpr(util::is_specialization<T,std::vector>::value)
		return Type::Array;
	else if constexpr(util::is_specialization<T,std::unordered_map>::value || util::is_specialization<T,std::map>::value)
		return Type::Element;
	else if constexpr(std::is_same_v<T,void>)
		return Type::Nil;
	else if constexpr(util::is_string<T>())
		return Type::String;
	else if constexpr(std::is_same_v<T,Utf8String>)
		return Type::Utf8String;
	else if constexpr(std::is_same_v<T,Int8>)
		return Type::Int8;
	else if constexpr(std::is_same_v<T,UInt8>)
		return Type::UInt8;
	else if constexpr(std::is_same_v<T,Int16>)
		return Type::Int16;
	else if constexpr(std::is_same_v<T,UInt16>)
		return Type::UInt16;
	else if constexpr(std::is_same_v<T,Int32>)
		return Type::Int32;
	else if constexpr(std::is_same_v<T,UInt32>)
		return Type::UInt32;
	else if constexpr(std::is_same_v<T,Int64>)
		return Type::Int64;
	else if constexpr(std::is_same_v<T,UInt64>)
		return Type::UInt64;
	else if constexpr(std::is_same_v<T,Float>)
		return Type::Float;
	else if constexpr(std::is_same_v<T,Double>)
		return Type::Double;
	else if constexpr(std::is_same_v<T,Vector2>)
		return Type::Vector2;
	else if constexpr(std::is_same_v<T,Vector3>)
		return Type::Vector3;
	else if constexpr(std::is_same_v<T,Vector4>)
		return Type::Vector4;
	else if constexpr(std::is_same_v<T,Quaternion>)
		return Type::Quaternion;
	else if constexpr(std::is_same_v<T,EulerAngles>)
		return Type::EulerAngles;
	else if constexpr(std::is_same_v<T,Srgba>)
		return Type::Srgba;
	else if constexpr(std::is_same_v<T,HdrColor>)
		return Type::HdrColor;
	else if constexpr(std::is_same_v<T,Boolean>)
		return Type::Boolean;
	else if constexpr(std::is_same_v<T,Transform>)
		return Type::Transform;
	else if constexpr(std::is_same_v<T,ScaledTransform>)
		return Type::ScaledTransform;
	else if constexpr(std::is_same_v<T,Mat4>)
		return Type::Mat4;
	else if constexpr(std::is_same_v<T,Mat3x4>)
		return Type::Mat3x4;
	else if constexpr(std::is_same_v<T,Blob>)
		return Type::Blob;
	else if constexpr(std::is_same_v<T,BlobLz4>)
		return Type::BlobLz4;
	else if constexpr(std::is_same_v<T,Element>)
		return Type::Element;
	else if constexpr(std::is_same_v<T,Array>)
		return Type::Array;
	else
		static_assert(false,"Unsupported type!");
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	return Type::Nil;
}

template<typename TFrom,typename TTo>
	constexpr bool udm::is_convertible()
{
	return std::is_convertible_v<TFrom,TTo>;
}
		
template<typename TTo>
	constexpr bool udm::is_convertible_from(Type tFrom)
{
	if(is_numeric_type(tFrom))
	{
		auto tag = get_numeric_tag(tFrom);
		return std::visit([&](auto tag){return is_convertible<decltype(tag)::type,TTo>();},tag);
	}

	if(is_generic_type(tFrom))
	{
		auto tag = get_generic_tag(tFrom);
		return std::visit([&](auto tag){return is_convertible<decltype(tag)::type,TTo>();},tag);
	}

	if(tFrom == Type::String)
		return is_convertible<String,TTo>();
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	return false;
}

template<typename TFrom>
	constexpr bool udm::is_convertible(Type tTo)
{
	if(is_numeric_type(tTo))
	{
		auto tag = get_numeric_tag(tTo);
		return std::visit([&](auto tag){return is_convertible<TFrom,decltype(tag)::type>();},tag);
	}

	if(is_generic_type(tTo))
	{
		auto tag = get_generic_tag(tTo);
		return std::visit([&](auto tag){return is_convertible<TFrom,decltype(tag)::type>();},tag);
	}

	if(tTo == Type::String)
		return is_convertible<TFrom,String>();
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	return false;
}

constexpr bool udm::is_convertible(Type tFrom,Type tTo)
{
	if(is_numeric_type(tFrom))
	{
		auto tag = get_numeric_tag(tFrom);
		return std::visit([&](auto tag){return is_convertible<decltype(tag)::type>(tTo);},tag);
	}

	if(is_generic_type(tTo))
	{
		auto tag = get_generic_tag(tFrom);
		return std::visit([&](auto tag){return is_convertible<decltype(tag)::type>(tTo);},tag);
	}

	if(tFrom == Type::String)
		return is_convertible<String>(tTo);
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	return false;
}

template<typename T>
	void udm::Property::operator=(T &&v)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if constexpr(util::is_specialization<TBase,std::vector>::value)
	{
		using TValueType = TBase::value_type;
		if(type != Type::Array)
			throw std::logic_error{"Attempted to assign vector to non-array property (of type " +std::string{magic_enum::enum_name(type)} +"), this is not allowed!"};
		auto valueType = type_to_enum<TValueType>();
		auto size = v.size();
		auto &a = *static_cast<Array*>(value);
		a.Clear();
		a.SetValueType(valueType);
		a.Resize(size);

		if(size_of_base_type(valueType) != sizeof(TBase::value_type))
			throw std::logic_error{"Type size mismatch!"};
		auto vs = [this,&a,&v](auto tag) {
			using TTag = decltype(tag)::type;
			memcpy(a.values,v.data(),v.size() *sizeof(v[0]));
		};
		if(is_numeric_type(valueType))
			std::visit(vs,get_numeric_tag(valueType));
		else if(is_generic_type(valueType))
			std::visit(vs,get_generic_tag(valueType));
		else if(is_non_trivial_type(valueType))
		{
			// Elements have to be copied explicitly
			for(auto i=decltype(size){0u};i<size;++i)
				a[i] = v[i];
		}
		return;
	}
	else if constexpr(util::is_specialization<TBase,std::unordered_map>::value || util::is_specialization<TBase,std::map>::value)
	{
		if(type != Type::Element)
			throw std::logic_error{"Attempted to assign map to non-element property (of type " +std::string{magic_enum::enum_name(type)} +"), this is not allowed!"};
		for(auto &pair : v)
			(*this)[pair.first] = pair.second;
		return;
	}
	auto vType = type_to_enum<TBase>();
	auto vs = [this,&v](auto tag) {
		using TTag = decltype(tag)::type;
		if constexpr(is_convertible<TBase,TTag>())
			*static_cast<TTag*>(value) = v;
	};
	if(is_numeric_type(vType))
		std::visit(vs,get_numeric_tag(vType));
	
	if(is_generic_type(vType))
		std::visit(vs,get_generic_tag(vType));
	
	if(is_non_trivial_type(vType))
		std::visit(vs,get_non_trivial_tag(vType));
}

template<typename T>
	void udm::LinkedPropertyWrapper::operator=(T &&v)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if(prop == nullptr)
		InitializeProperty();
	if(prev && prev->arrayIndex != std::numeric_limits<uint32_t>::max() && prev->prev && prev->prev->prop && prev->prev->prop->type == Type::Array)
	{
		(*static_cast<Array*>(prev->prev->prop->value))[prev->arrayIndex][propName] = v;
		return;
	}
	PropertyWrapper::operator=(v);
}
template<typename T>
	void udm::PropertyWrapper::operator=(T &&v)
{
	if(prop == nullptr)
		throw std::runtime_error{"Cannot assign propety value: Property is invalid!"};
	if(prop->type == Type::Array)
	{
		if(arrayIndex == std::numeric_limits<uint32_t>::max())
			throw std::runtime_error{"Cannot assign propety value to array: No index has been specified!"};
		if(linked && !static_cast<LinkedPropertyWrapper*>(this)->propName.empty())
		{
			auto &a = *static_cast<Array*>(prop->value);
			if(a.valueType != Type::Element)
				return;
			a.GetValue<Element>(arrayIndex).children[static_cast<LinkedPropertyWrapper*>(this)->propName] = Property::Create(v);
		}
		else
			static_cast<Array*>(prop->value)->SetValue(arrayIndex,v);
		return;
	}
	if(prop->type != Type::Element)
	{
		*prop = v;
		return;
	}
	auto &el = *static_cast<Element*>(prop->value);
	auto &wpParent = el.parentProperty;
	if(!wpParent)
		throw std::runtime_error{"Attempted to change value of element property without a valid parent, this is not allowed!"};
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
		throw std::runtime_error{
			"Element has parent of type " +std::string{magic_enum::enum_name(parent->type)} +", but only " +std::string{magic_enum::enum_name(Type::Element)}/* +" and " +std::string{magic_enum::enum_name(Type::Array)}*/ +" types are allowed!"
		};
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
	T &udm::Array::GetValue(uint32_t idx)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	auto vs = [this,idx](auto tag) -> T& {
		using TTag = decltype(tag)::type;
		if constexpr(std::is_same_v<TTag,TBase>)
			return static_cast<TTag*>(values)[idx];
		throw std::logic_error{"Attempted to retrieve value of type " +std::string{magic_enum::enum_name(type_to_enum<T>())} +" from array of type " +std::string{magic_enum::enum_name(valueType)} +"!"};
	};
	if(is_numeric_type(valueType))
		return std::visit(vs,get_numeric_tag(valueType));
	if(is_generic_type(valueType))
		return std::visit(vs,get_generic_tag(valueType));
	if(is_non_trivial_type(valueType))
		return std::visit(vs,get_non_trivial_tag(valueType));
}

template<typename T>
	void udm::Array::SetValue(uint32_t idx,T &&v)
{
	using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
	if(!is_convertible<TBase>(valueType))
	{
		throw std::runtime_error{
			"Attempted to insert value of type " +std::string{magic_enum::enum_name(type_to_enum<TBase>())} +" into array of type " +std::string{magic_enum::enum_name(valueType)} +", which are not compatible!"
		};
	}

	auto vs = [this,idx,&v](auto tag) {
		using TTag = decltype(tag)::type;
		if constexpr(is_convertible<TBase,TTag>())
			static_cast<TTag*>(values)[idx] = v;
	};
	if(is_numeric_type(valueType))
	{
		std::visit(vs,get_numeric_tag(valueType));
		return;
	}
	
	if(is_generic_type(valueType))
	{
		std::visit(vs,get_generic_tag(valueType));
		return;
	}
	
	if(is_non_trivial_type(valueType))
	{
		std::visit(vs,get_non_trivial_tag(valueType));
		return;
	}
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
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
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
		if(type != Type::Array)
			return {};
		auto &a = GetValue<Array>();
		if(a.IsValueType(array_value_type_to_enum<T>()) == false)
			return {};
		auto n = a.GetSize();
		T v {};
		v.resize(n);
		if constexpr(is_non_trivial_type(array_value_type_to_enum<T>()))
		{
			for(auto i=decltype(n){0u};i<n;++i)
				v[i] = static_cast<T::value_type*>(a.values)[i];
			return v;
		}
		else
		{
			memcpy(v.data(),a.values,v.size() *sizeof(v.front()));
			return v;
		}
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
			return static_cast<T>(const_cast<udm::Property*>(this)->GetValue<decltype(tag)::type>());
		return {};
	};
	if(is_numeric_type(type))
		return std::visit(vs,get_numeric_tag(type));
	
	if(is_generic_type(type))
		return std::visit(vs,get_generic_tag(type));
	
	if(is_non_trivial_type(type))
		return std::visit(vs,get_non_trivial_tag(type));
	static_assert(umath::to_integral(Type::Count) == 29,"Update this list when new types are added!");
	return {};
}

template<typename T>
	T &udm::Property::GetValue(Type type)
{
	assert(data && this->type == type);
	if(this->type != type)
		throw std::runtime_error{"Type mismatch, requested type is " +std::string{magic_enum::enum_name(type)} +", but actual type is " +std::string{magic_enum::enum_name(this->type)} +"!"};
	return *GetValuePtr<T>();
}

template<typename T>
	T *udm::Property::GetValuePtr()
{
	return (this->type == type_to_enum<T>()) ? reinterpret_cast<T*>(value) : nullptr;
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
	T &udm::PropertyWrapper::GetValue()
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValue<T>();
		if(a.IsValueType(type_to_enum<T>()) == false)
			throw std::runtime_error{"Type mismatch, requested type is " +std::string{magic_enum::enum_name(type_to_enum<T>())} +", but actual type is " +std::string{magic_enum::enum_name(a.valueType)} +"!"};
		return static_cast<T*>(a.values)[arrayIndex];
	}
	return (*this)->GetValue<T>();
}
template<typename T>
	const T &udm::PropertyWrapper::GetValue() const {return const_cast<PropertyWrapper*>(this)->GetValue<T>();}

template<typename T>
	T *udm::PropertyWrapper::GetValuePtr()
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValuePtr<T>();
		if(a.IsValueType(type_to_enum<T>()) == false)
			return nullptr;
		return &static_cast<T*>(a.values)[arrayIndex];
	}
	return prop ? (*this)->GetValuePtr<T>() : nullptr;
}
template<typename T>
	T udm::PropertyWrapper::ToValue(const T &defaultValue) const
{
	if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
		return defaultValue;
	auto val = ToValue<T>();
	return val.has_value() ? std::move(val.value()) : defaultValue;
}

template<typename T>
	udm::ArrayIterator<T> udm::PropertyWrapper::begin()
{
	if(!static_cast<bool>(*this))
		return ArrayIterator<T>{};
	auto *a = GetValuePtr<Array>();
	if(a == nullptr)
		return ArrayIterator<T>{};
	return a->begin<T>();
}
template<typename T>
	udm::ArrayIterator<T> udm::PropertyWrapper::end()
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
			if constexpr(is_convertible<decltype(tag)::type>(type_to_enum<T>()))
				return static_cast<T>(const_cast<udm::PropertyWrapper*>(this)->GetValue<decltype(tag)::type>());
			return {};
		};
		if(is_numeric_type(a.valueType))
			return std::visit(vs,get_numeric_tag(a.valueType));
	
		if(is_generic_type(a.valueType))
			return std::visit(vs,get_generic_tag(a.valueType));
	
		if(is_non_trivial_type(a.valueType))
			return std::visit(vs,get_non_trivial_tag(a.valueType));
		return {}; // Unreachable
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
	typename udm::ArrayIterator<T>::reference udm::ArrayIterator<T>::operator*() {return m_curProperty.GetValue<T>();}

template<typename T>
	typename udm::ArrayIterator<T>::pointer udm::ArrayIterator<T>::operator->() {return m_curProperty.GetValuePtr<T>();}

template<typename T>
	bool udm::ArrayIterator<T>::operator==(const ArrayIterator &other) const {return m_curProperty == other.m_curProperty;}

template<typename T>
	bool udm::ArrayIterator<T>::operator!=(const ArrayIterator &other) const {return !operator==(other);}

#endif
