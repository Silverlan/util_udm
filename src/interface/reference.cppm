// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include "sharedutils/magic_enum.hpp"

export module pragma.udm:reference;

export import :exception;
export import :types;

export {
	namespace udm {
		struct DLLUDM Reference {
			static constexpr std::uint32_t layout_version = 1; // Increment this whenever members of this class are changed
			
			Reference() = default;
			Reference(const std::string &path) : path {path} {}
			Reference(const Reference &other) : property {other.property}, path {other.path} {}
			Reference(Reference &&other) : property {other.property}, path {std::move(other.path)} {}
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
			bool operator!=(const Reference &other) const { return !operator==(other); }
		private:
			friend Data;
			void InitializeProperty(const LinkedPropertyWrapper &root);
		};
	}
}
