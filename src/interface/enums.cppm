// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.udm:enums;

import pragma.util;

export {
	namespace udm {
		enum class Type : uint8_t {
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
			Last = Count - 1,
			Invalid = std::numeric_limits<uint8_t>::max()
		};

		enum class ArrayType : uint8_t {
			Raw = 0,
			Compressed,
		};

		enum class BlobResult : uint8_t {
			Success = 0,
			DecompressedSizeMismatch,
			InsufficientSize,
			ValueTypeMismatch,
			NotABlobType,
			InvalidProperty,
		};

		enum class MergeFlags : uint32_t {
			None = 0u,
			OverwriteExisting = 1u,
			DeepCopy = OverwriteExisting << 1u,
		};

		enum class FormatType : uint8_t {
			Binary = 0,
			Ascii,
		};

		enum class AsciiSaveFlags : uint32_t {
			None = 0u,
			IncludeHeader = 1u,
			DontCompressLz4Arrays = IncludeHeader << 1u,
			Default = None,
		};

		constexpr const char *enum_type_to_ascii(Type t)
		{
			// Note: These have to match ascii_type_to_enum
			switch(t) {
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
			default:
				break;
			}
			static_assert(umath::to_integral(Type::Count) == 36, "Update this list when new types are added!");
			return nullptr;
		}
		DLLUDM Type ascii_type_to_enum(const std::string_view &type);

		template<typename TEnum>
		constexpr std::string_view enum_to_string(TEnum e)
		{
			return magic_enum::enum_name(e);
		}

		template<typename TEnum>
		constexpr std::string flags_to_string(TEnum e)
		{
			return magic_enum::enum_flags_name(e);
		}

		using namespace umath::scoped_enum::bitwise;
	};

	namespace umath::scoped_enum::bitwise {
		template<>
		struct enable_bitwise_operators<udm::AsciiSaveFlags> : std::true_type {};

		template<>
		struct enable_bitwise_operators<udm::MergeFlags> : std::true_type {};
	}
}
