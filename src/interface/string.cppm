// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include "sharedutils/magic_enum.hpp"
#include <string>
#include <vector>

export module pragma.udm:types.string;

import :exception;

export {
	namespace udm {
		struct DLLUDM Utf8String {
			Utf8String() = default;
			Utf8String(std::vector<uint8_t> &&data) : data {data} {}
			Utf8String(const Utf8String &str) : data {str.data} {}
			std::vector<uint8_t> data;

			Utf8String &operator=(Utf8String &&other);
			Utf8String &operator=(const Utf8String &other);

			bool operator==(const Utf8String &other) const
			{
				auto res = (data == other.data);
				UDM_ASSERT_COMPARISON(res);
				return res;
			}
			bool operator!=(const Utf8String &other) const { return !operator==(other); }
		};
	}
}
