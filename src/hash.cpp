// SPDX-FileCopyrightText: © 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "udm.hpp"
#include <sharedutils/util_hash.hpp>

inline void hash_combine(util::MurmurHash3 &seed, util::MurmurHash3 const &h)
{
	static_assert(sizeof(util::MurmurHash3) % sizeof(uint32_t) == 0,
				  "MurmurHash3 must be a multiple of 4 bytes");
	// reinterpret the bytes as four 32‑bit words:
	uint32_t*       s = reinterpret_cast<uint32_t*>(seed.data());
	uint32_t const* hh= reinterpret_cast<uint32_t const*>(h.data());

	// 0x9e3779b9 is the 32‑bit fraction of the golden ratio
	for (int i = 0; i < 4; ++i) {
		s[i] ^= hh[i]
			  + 0x9e3779b9
			  + (s[i] << 6)
			  + (s[i] >> 2);
	}
}

static constexpr uint32_t MURMUR_SEED = 195574;
template<typename T>
static util::MurmurHash3 hash_basic_type(const T &v) {
	return util::murmur_hash3(&v, sizeof(v), MURMUR_SEED);
}

static util::MurmurHash3 hash(const udm::String &str);
static util::MurmurHash3 hash(const udm::Utf8String &str);
static util::MurmurHash3 hash(const udm::Blob &v);
static util::MurmurHash3 hash(const udm::BlobLz4 &v);
static util::MurmurHash3 hash(const udm::Array &v);
static util::MurmurHash3 hash(const udm::ArrayLz4 &v);
static util::MurmurHash3 hash(const udm::Reference &v);
static util::MurmurHash3 hash(const udm::Struct &v);
static util::MurmurHash3 hash(const udm::Element &e);
static util::MurmurHash3 hash(const udm::Property &prop);

util::MurmurHash3 hash(const udm::String &str) {
	return util::murmur_hash3(str.data(), str.length(), MURMUR_SEED);
}

util::MurmurHash3 hash(const std::string_view &str) {
	return util::murmur_hash3(str.data(), str.length(), MURMUR_SEED);
}

util::MurmurHash3 hash(const udm::Utf8String &str) {
	return util::murmur_hash3(str.data.data(), str.data.size(), MURMUR_SEED);
}

util::MurmurHash3 hash(const udm::Blob &v) {
	return util::murmur_hash3(v.data.data(), v.data.size(), MURMUR_SEED);
}

util::MurmurHash3 hash(const udm::BlobLz4 &v) {
	return util::murmur_hash3(v.compressedData.data(), v.compressedData.size(), MURMUR_SEED);
}

util::MurmurHash3 hash(const udm::Array &v) {
	auto valueType = v.GetValueType();
	if (udm::is_trivial_type(valueType)) {
		auto *ptr = v.GetValues();
		return util::murmur_hash3(ptr, v.GetByteSize(), MURMUR_SEED);
	}
	util::MurmurHash3 hashVal {};
	std::fill(hashVal.begin(), hashVal.end(), 0);
	udm::visit(valueType, [&](auto tag) {
		using T = typename decltype(tag)::type;
		if constexpr(udm::is_non_trivial_type(udm::type_to_enum<T>())) {
			for (auto &val : const_cast<udm::Array&>(v)) {
				auto &v = val.GetValue<T>();
				hash_combine(hashVal, hash(v));
			}
		}
	});
	return hashVal;
}

util::MurmurHash3 hash(const udm::ArrayLz4 &v) {
	auto &blob = v.GetCompressedBlob();
	return util::murmur_hash3(blob.compressedData.data(), blob.compressedData.size(), MURMUR_SEED);
}

util::MurmurHash3 hash(const udm::Reference &v) {
	return util::murmur_hash3(v.path.data(), v.path.length(), MURMUR_SEED);
}

util::MurmurHash3 hash(const udm::Struct &v) {
	auto &desc = v.description;
	util::MurmurHash3 hashVal {};
	std::fill(hashVal.begin(), hashVal.end(), 0);
	for (auto &name : desc.names)
		hash_combine(hashVal, hash(name));
	for (auto type : desc.types)
		hash_combine(hashVal, hash_basic_type(type));
	hash_combine(hashVal, util::murmur_hash3(v.data.data(), v.data.size(), MURMUR_SEED));
	return hashVal;
}

util::MurmurHash3 hash(const udm::Property &prop) {
	return udm::visit(prop.type, [&](auto tag) {
		using T = typename decltype(tag)::type;
		auto &val = prop.GetValue<T>();
		if constexpr(udm::is_trivial_type(udm::type_to_enum<T>()))
			return hash_basic_type(val);
		else
			return hash(val);
	});
}

util::MurmurHash3 hash(const udm::Element &e) {
	util::MurmurHash3 hashVal {};
	std::fill(hashVal.begin(), hashVal.end(), 0);
	std::vector<std::pair<std::string_view, const udm::PProperty*>> sortedKeys;
	sortedKeys.reserve(e.children.size());
	for (auto &[key, child] : e.children)
		sortedKeys.emplace_back(key, &child);
	std::sort(sortedKeys.begin(), sortedKeys.end(), [](auto &a, auto &b) {
		return a.first < b.first;
	});
	for (auto &[key, prop] : sortedKeys) {
		auto &child = *prop;
		auto hash0 = hash(key);
		auto hash1 = hash(*child);
		hash_combine(hashVal, hash0);
		hash_combine(hashVal, hash1);
	}
	return hashVal;
}

util::MurmurHash3 hash(const udm::PropertyWrapper &e)
{
	return udm::visit(e.GetType(), [&](auto tag) {
		using T = typename decltype(tag)::type;
		auto &val = e.GetValue<T>();
		if constexpr(udm::is_trivial_type(udm::type_to_enum<T>()))
			return hash_basic_type(val);
		else
			return hash(val);
	});
}

udm::Hash udm::Property::CalcHash() const { return hash(*this); }
udm::Hash udm::PropertyWrapper::CalcHash() const { return hash(*this); }
