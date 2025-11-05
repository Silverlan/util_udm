// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.udm:half;

export import std.compat;

export {
	namespace udm {
	#pragma pack(push, 1)
		struct DLLUDM Half {
			Half() = default;
			Half(uint16_t value) : value {value} {}
			Half(const Half &other) = default;
			Half(float value);
			operator float() const;
			Half &operator=(float value);
			Half &operator=(uint16_t value);
			Half &operator=(const Half &other) = default;
			uint16_t value;
		};
	#pragma pack(pop)
		static_assert(sizeof(Half) == sizeof(uint16_t));
	};
}
