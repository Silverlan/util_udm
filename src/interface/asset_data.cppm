// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include "sharedutils/magic_enum.hpp"

export module pragma.udm:asset_data;

import :core;
export import :property_wrapper;

export {
	namespace udm {
		struct DLLUDM AssetData : public LinkedPropertyWrapper {
			std::string GetAssetType() const;
			Version GetAssetVersion() const;
			void SetAssetType(const std::string &assetType) const;
			void SetAssetVersion(Version version) const;

			LinkedPropertyWrapper GetData() const;
			LinkedPropertyWrapper operator*() const { return GetData(); }
			LinkedPropertyWrapper operator->() const { return GetData(); }
		};
	}
}
