// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <cassert>

module pragma.udm;

#ifndef UDM_SINGLE_MODULE_INTERFACE
import :core;
#endif

static void to_json(udm::LinkedPropertyWrapperArg prop, std::stringstream &ss, const std::string &t)
{
	auto type = prop.GetType();
	auto tsub = t + '\t';
	if(udm::is_array_type(type)) {
		ss << "[";
		auto first = true;
		auto addNewLine = false;
		for(auto &item : prop) {
			if(first)
				first = false;
			else
				ss << ",";
			if(item.IsType(udm::Type::Element)) {
				addNewLine = true;
				ss << "\n" << tsub;
			}
			to_json(item, ss, tsub);
		}
		if(addNewLine)
			ss << "\n" << t;
		ss << "]";
		return;
	}

	if(prop.IsType(udm::Type::Element)) {
		ss << "{\n";
		auto first = true;
		for(auto &pair : const_cast<udm::LinkedPropertyWrapper &>(prop).ElIt()) {
			if(first)
				first = false;
			else
				ss << ",\n";
			ss << tsub << "\"" << pair.key << "\": ";
			to_json(pair.property, ss, tsub);
		}
		ss << "\n" << t << "}";
		return;
	}

	if(prop.GetType() == udm::Type::Blob || prop.GetType() == udm::Type::BlobLz4) {
		uint64_t blobSize;
		auto res = prop.GetBlobData(nullptr, 0ull, &blobSize);
		if(res == udm::BlobResult::InsufficientSize) {
			std::vector<uint8_t> blobData;
			blobData.resize(blobSize);
			auto res = prop.GetBlobData(blobData.data(), blobData.size());
			if(res == udm::BlobResult::Success) {
				auto encoded = util::base64_encode(blobData.data(), blobData.size());
				ss << "\"" << encoded << "\"";
				return;
			}
		}
		ss << "\"\"";
		return;
	}

	auto strVal = prop.ToValue<udm::String>();
	if(!strVal.has_value())
		std::cout << "";
	assert(strVal.has_value());
	if(strVal.has_value())
		ss << "\"" << *strVal << "\"";
}

void udm::to_json(LinkedPropertyWrapperArg prop, std::stringstream &ss) { ::to_json(prop, ss, ""); }
