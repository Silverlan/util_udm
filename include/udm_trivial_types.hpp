/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_TRIVIAL_TYPES_HPP__
#define __UDM_TRIVIAL_TYPES_HPP__

#include <cinttypes>
#include <string>
#include <variant>
#include <map>
#include <mathutil/uvec.h>
#include <mathutil/transform.hpp>
#include "udm_exception.hpp"

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

#pragma pack(push,1)
	struct Half
	{
		Half()=default;
		Half(uint16_t value)
			: value{value}
		{}
		Half(const Half &other)=default;
		Half(float value);
		operator float() const;
		Half &operator=(float value);
		Half &operator=(uint16_t value);
		Half &operator=(const Half &other)=default;
		uint16_t value;
	};
#pragma pack(pop)
	static_assert(sizeof(Half) == sizeof(uint16_t));

	using Float = float;
	using Double = double;
	using Boolean = bool;

	using Vector2 = ::Vector2;
	using Vector3 = ::Vector3;
	using Vector4 = ::Vector4;
	using Vector2i = ::Vector2i;
	using Vector3i = ::Vector3i;
	using Vector4i = ::Vector4i;
	using Quaternion = Quat;
	using EulerAngles = ::EulerAngles;
	using Srgba = std::array<uint8_t,4>;
	using HdrColor = std::array<uint16_t,3>;
	using Transform = umath::Transform;
	using ScaledTransform = umath::ScaledTransform;
	using Mat4 = Mat4;
	using Mat3x4 = Mat3x4;
	using Nil = std::monostate;

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
		ArrayLz4,
		Reference,
		Struct,
		Half,
		Vector2i,
		Vector3i,
		Vector4i,

		Count,
		Last = Count -1,
		Invalid = std::numeric_limits<uint8_t>::max()
	};
	static constexpr std::array<Type,12> NUMERIC_TYPES = {
		Type::Int8,Type::UInt8,
		Type::Int16,Type::UInt16,
		Type::Int32,Type::UInt32,
		Type::Int64,Type::UInt64,
		Type::Float,Type::Double,
		Type::Boolean,Type::Half
	};
	static constexpr std::array<Type,15> GENERIC_TYPES = {
		Type::Vector2,Type::Vector3,Type::Vector4,Type::Vector2i,Type::Vector3i,Type::Vector4i,Type::Quaternion,
		Type::EulerAngles,Type::Srgba,Type::HdrColor,Type::Transform,Type::ScaledTransform,Type::Mat4,Type::Mat3x4,Type::Nil
	};
	static constexpr std::array<Type,9> NON_TRIVIAL_TYPES = {Type::String,Type::Utf8String,Type::Blob,Type::BlobLz4,Type::Element,Type::Array,Type::ArrayLz4,Type::Reference,Type::Struct};

	template<typename T>
		concept is_vector_type = std::is_same_v<T,Vector2> || std::is_same_v<T,Vector2i> || std::is_same_v<T,Vector3> || std::is_same_v<T,Vector3i> || std::is_same_v<T,Vector4> || std::is_same_v<T,Vector4i>;
	template<typename T>
		concept is_matrix_type = std::is_same_v<T,Mat4> || std::is_same_v<T,Mat3x4>;
	template<typename T>
		concept is_arithmetic = std::is_arithmetic_v<T> || std::is_same_v<T,Half>;

	template<typename T>
		using underlying_numeric_type = 
			std::conditional_t<std::is_same_v<T,Half>,uint16_t,
			std::conditional_t<is_arithmetic<T>,T,
			std::conditional_t<is_vector_type<T> || is_matrix_type<T> || std::is_same_v<T,Quaternion>,float,
			std::conditional_t<std::is_same_v<T,Srgba>,uint8_t,
			std::conditional_t<std::is_same_v<T,HdrColor>,uint16_t,
			std::conditional_t<std::is_same_v<T,EulerAngles>,float,
			void
		>>>>>>;

	template<typename T>
		constexpr Type type_to_enum();
	template<typename T>
		constexpr Type type_to_enum_s();

	constexpr size_t size_of(Type t);
	constexpr size_t size_of_base_type(Type t);
	template<typename T>
		constexpr Type array_value_type_to_enum();

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
		case Type::Half:
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
		case Type::Vector2i:
		case Type::Vector3i:
		case Type::Vector4i:
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

	static constexpr uint8_t get_numeric_component_count(udm::Type type)
	{
		if(udm::is_numeric_type(type))
			return 1;
		switch(type)
		{
		case udm::Type::Vector2:
		case udm::Type::Vector2i:
			return 2;
		case udm::Type::Vector3:
		case udm::Type::Vector3i:
		case udm::Type::EulerAngles:
		case udm::Type::HdrColor:
			return 3;
		case udm::Type::Vector4:
		case udm::Type::Vector4i:
		case udm::Type::Quaternion:
		case udm::Type::Srgba:
			return 4;
		case udm::Type::Transform:
			return 7;
		case udm::Type::ScaledTransform:
			return 10;
		case udm::Type::Mat3x4:
			return 12;
		case udm::Type::Mat4:
			return 16;
		}
		return 0;
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
		case Type::ArrayLz4:
		case Type::Reference:
		case Type::Struct:
			return true;
	  }
	  static_assert(NON_TRIVIAL_TYPES.size() == 9,"Update this list when new non-trivial types have been added!");
	  return false;
	}

	template<bool ENABLE_NUMERIC=true,bool ENABLE_GENERIC=true,bool ENABLE_NON_TRIVIAL=true>
		constexpr bool is_type(Type type)
	{
		if constexpr(ENABLE_NUMERIC)
		{
			if(is_numeric_type(type))
				return true;
		}
		if constexpr(ENABLE_GENERIC)
		{
			if(is_generic_type(type))
				return true;
		}
		if constexpr(ENABLE_NON_TRIVIAL)
		{
			if(is_non_trivial_type(type))
				return true;
		}
		return false;
	}
	constexpr bool is_ng_type(Type type) {return is_type<true,true,false>(type);}
	constexpr bool is_gnt_type(Type type) {return is_type<false,true,true>(type);}

	constexpr bool is_array_type(Type t)
	{
		switch(t)
		{
		case Type::Array:
		case Type::ArrayLz4:
			return true;
		}
		return false;
	}

	constexpr bool is_trivial_type(Type t) {return !is_non_trivial_type(t) && t != Type::Invalid;}

	template<class T>struct tag_t{using type=T;};
	template<class T>constexpr tag_t<T> tag={};
	constexpr std::variant<tag_t<Int8>,tag_t<UInt8>,tag_t<Int16>,tag_t<UInt16>,tag_t<Int32>,tag_t<UInt32>,tag_t<Int64>,tag_t<UInt64>,tag_t<Float>,tag_t<Double>,tag_t<Boolean>,tag_t<Half>> get_numeric_tag(Type e)
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
			case Type::Half: return tag<Half>;
		}
	}

	constexpr std::variant<
		tag_t<Vector2>,tag_t<Vector3>,tag_t<Vector4>,tag_t<Vector2i>,tag_t<Vector3i>,tag_t<Vector4i>,
		tag_t<Quaternion>,tag_t<EulerAngles>,tag_t<Srgba>,
		tag_t<HdrColor>,tag_t<Transform>,tag_t<ScaledTransform>,tag_t<Mat4>,tag_t<Mat3x4>,tag_t<Nil>
	> get_generic_tag(Type e)
	{
		switch(e)
		{
			case Type::Vector2: return tag<Vector2>;
			case Type::Vector3: return tag<Vector3>;
			case Type::Vector4: return tag<Vector4>;
			case Type::Vector2i: return tag<Vector2i>;
			case Type::Vector3i: return tag<Vector3i>;
			case Type::Vector4i: return tag<Vector4i>;
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
	struct ArrayLz4;
	struct Reference;
	struct Struct;
	constexpr std::variant<
		tag_t<String>,tag_t<Utf8String>,tag_t<Blob>,tag_t<BlobLz4>,tag_t<Element>,tag_t<Array>,
		tag_t<ArrayLz4>,tag_t<Reference>,tag_t<Struct>
	> get_non_trivial_tag(Type e)
	{
		switch(e)
		{
			case Type::String: return tag<String>;
			case Type::Utf8String: return tag<Utf8String>;
			case Type::Blob: return tag<Blob>;
			case Type::BlobLz4: return tag<BlobLz4>;
			case Type::Element: return tag<Element>;
			case Type::Array: return tag<Array>;
			case Type::ArrayLz4: return tag<ArrayLz4>;
			case Type::Reference: return tag<Reference>;
			case Type::Struct: return tag<Struct>;
		}
		static_assert(NON_TRIVIAL_TYPES.size() == 9,"Update this list when new non-trivial types have been added!");
	}

	template<bool ENABLE_NUMERIC=true,bool ENABLE_GENERIC=true,bool ENABLE_NON_TRIVIAL=true,bool ENABLE_DEFAULT_RETURN=true,typename T> requires(ENABLE_NUMERIC || ENABLE_GENERIC || ENABLE_NON_TRIVIAL)
		constexpr decltype(auto) visit(Type type,T vs)
		{
			if constexpr(ENABLE_NUMERIC)
			{
				if(is_numeric_type(type))
					return std::visit(vs,get_numeric_tag(type));
			}
			if constexpr(ENABLE_GENERIC)
			{
				if(is_generic_type(type))
					return std::visit(vs,get_generic_tag(type));
			}
			if constexpr(ENABLE_NON_TRIVIAL)
			{
				if(is_non_trivial_type(type))
					return std::visit(vs,get_non_trivial_tag(type));
			}
			// Unreachable if all template types enabled

			// Failure case, return default value
			if constexpr(ENABLE_DEFAULT_RETURN && (!ENABLE_NUMERIC || !ENABLE_GENERIC || !ENABLE_NON_TRIVIAL))
			{
				if constexpr(ENABLE_NUMERIC)
				{
					if(is_numeric_type(type))
					{
						auto tag = get_numeric_tag(type);
						return decltype(std::visit(vs,get_numeric_tag(type)))();
					}
				}
				if constexpr(ENABLE_GENERIC)
				{
					if(is_generic_type(type))
					{
						auto tag = get_generic_tag(type);
						return decltype(std::visit(vs,get_generic_tag(type)))();
					}
				}
				if constexpr(ENABLE_NON_TRIVIAL)
				{
					if(is_non_trivial_type(type))
					{
						auto tag = get_non_trivial_tag(type);
						return decltype(std::visit(vs,get_non_trivial_tag(type)))();
					}
				}
			}
		}
	template<bool ENABLE_DEFAULT_RETURN=true>
	constexpr decltype(auto) visit_ng(Type type,auto vs)
	{
		return visit<true,true,false,ENABLE_DEFAULT_RETURN>(type,vs);
	}
	template<bool ENABLE_DEFAULT_RETURN=true>
	constexpr decltype(auto) visit_gnt(Type type,auto vs)
	{
		return visit<false,true,true,ENABLE_DEFAULT_RETURN>(type,vs);
	}
};

template<typename T>
	constexpr udm::Type udm::type_to_enum()
{
	constexpr auto type = type_to_enum_s<T>();
	if constexpr(umath::to_integral(type) > umath::to_integral(Type::Last))
		static_assert(false,"Unsupported type!");
	return type;
}

template<typename T>
	constexpr udm::Type udm::type_to_enum_s()
{
	if constexpr(std::is_enum_v<T>)
		return type_to_enum_s<std::underlying_type_t<T>>();

	if constexpr(util::is_specialization<T,std::vector>::value)
		return Type::Array;
	else if constexpr(util::is_specialization<T,std::unordered_map>::value || util::is_specialization<T,std::map>::value)
		return Type::Element;
	else if constexpr(std::is_same_v<T,Nil> || std::is_same_v<T,void>)
		return Type::Nil;
	else if constexpr(util::is_string<T>::value || std::is_same_v<T,std::string_view>)
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
	else if constexpr(std::is_same_v<T,Vector2i>)
		return Type::Vector2i;
	else if constexpr(std::is_same_v<T,Vector3>)
		return Type::Vector3;
	else if constexpr(std::is_same_v<T,Vector3i>)
		return Type::Vector3i;
	else if constexpr(std::is_same_v<T,Vector4>)
		return Type::Vector4;
	else if constexpr(std::is_same_v<T,Vector4i>)
		return Type::Vector4i;
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
	else if constexpr(std::is_same_v<T,ArrayLz4>)
		return Type::ArrayLz4;
	else if constexpr(std::is_same_v<T,Reference>)
		return Type::Reference;
	else if constexpr(std::is_same_v<T,Half>)
		return Type::Half;
	else if constexpr(std::is_same_v<T,Struct>)
		return Type::Struct;
	static_assert(umath::to_integral(Type::Count) == 36,"Update this list when new types are added!");
	return Type::Invalid;
}

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
	throw InvalidUsageError{std::string{"UDM type "} +std::string{magic_enum::enum_name(t)} +" has non-constant size!"};
	static_assert(umath::to_integral(Type::Count) == 36,"Update this list when new types are added!");
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

#endif
