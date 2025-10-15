// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

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
#include <sharedutils/magic_enum.hpp>

export module pragma.udm:core;

#undef VERSION

export {
	namespace udm {
		using Version = uint32_t;
		/* Version history:
		* 1: Initial version
		* 2: Added types: reference, arrayLz4, struct, half, vector2i, vector3i, vector4i
		*/
		constexpr Version VERSION = 2;
		constexpr auto *HEADER_IDENTIFIER = "UDMB";
	#pragma pack(push, 1)
		struct DLLUDM Header {
			Header() = default;
			std::array<char, 4> identifier = {HEADER_IDENTIFIER[0], HEADER_IDENTIFIER[1], HEADER_IDENTIFIER[2], HEADER_IDENTIFIER[3]};
			Version version = VERSION;
		};
	#pragma pack(pop)

		namespace detail {
			DLLUDM void test_c_wrapper();
		};
	};
}
