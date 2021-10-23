/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_BASIC_TYPES_HPP__
#define __UDM_BASIC_TYPES_HPP__

#include <string>
#include <cinttypes>
#include <mathutil/uvec.h>
#include <mathutil/transform.hpp>

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
};

#endif
