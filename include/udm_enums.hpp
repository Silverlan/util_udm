// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __UDM_ENUMS_HPP__
#define __UDM_ENUMS_HPP__

#include <cstdint>
#include <limits>

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
};
#endif
