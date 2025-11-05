// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;


#include "definitions.hpp"
#include <cassert>

module pragma.udm;

#ifndef UDM_SINGLE_MODULE_INTERFACE
import :core;
#endif

struct BaseUdmData {
	BaseUdmData(const std::shared_ptr<udm::Data> &udmData, bool clearDataOnDestruction) : data {udmData}, clearDataOnDestruction {clearDataOnDestruction} {}
	~BaseUdmData() { FreeMemory(); }
	std::shared_ptr<udm::Data> data;
	bool clearDataOnDestruction = false;
	void FreeMemory()
	{
		for(auto &deleter : deleters)
			deleter();
		deleters.clear();
	}
	void AddDeleter(const std::function<void()> &deleter)
	{
		if(!clearDataOnDestruction)
			return;
		if(deleters.size() == deleters.capacity())
			deleters.reserve(deleters.size() * 2.f + 50);
		deleters.push_back(deleter);
	}
	std::vector<std::function<void()>> deleters;

	udm::Data *operator->() { return data.get(); }
	udm::Data &operator*() { return *data; }
};
struct BaseProperty {
	BaseProperty(BaseUdmData &data, udm::LinkedPropertyWrapper prop) : data {data}, prop {prop} {}
	BaseUdmData &data;
	udm::LinkedPropertyWrapper prop;

	udm::Property *operator->() { return prop.operator->(); }
	udm::Property &operator*() { return prop.operator*(); }
};
using UdmData = BaseUdmData *;
using UdmProperty = BaseProperty *;
using UdmType = std::underlying_type_t<udm::Type>;
using UdmArrayType = std::underlying_type_t<udm::ArrayType>;
struct BaseUdmElementIterator {
	std::unique_ptr<udm::LinkedPropertyWrapper> baseProp;
	UdmProperty prop;
	udm::ElementIteratorWrapper wrapper;
	udm::ElementIterator it;
};
using UdmElementIterator = BaseUdmElementIterator *;
template<typename T>
using UdmUnderlyingType = std::conditional_t<std::is_same_v<T, udm::Transform> || std::is_same_v<T, udm::ScaledTransform>, float, udm::underlying_numeric_type<T>>;
static char *to_cstring(const std::string &str)
{
	auto *cstr = new char[str.length() + 1];
	strcpy(cstr, str.data());
	return cstr;
}
static char *to_cstring(BaseUdmData &data, const std::string &str)
{
	auto *cstr = new char[str.length() + 1];
	strcpy(cstr, str.data());
	data.AddDeleter([cstr]() { delete[] cstr; });
	return cstr;
}
static udm::LinkedPropertyWrapper get_property(UdmProperty udmData, const char *path)
{
	if(!path || strlen(path) == 0)
		return udmData->prop;
	return udmData->prop.GetFromPath(path);
}
template<typename T>
static T udm_read_property_t(UdmProperty udmData, const char *path, T defaultValue)
{
	auto prop = get_property(udmData, path);
	if(!prop)
		return defaultValue;
	auto val = prop.ToValue<T>();
	if(val.has_value())
		return *val;
	return defaultValue;
}
template<typename T>
static void udm_write_property_t(UdmProperty udmData, const char *path, T value)
{
	auto prop = (strlen(path) > 0) ? udmData->prop.Add(path, udm::type_to_enum<T>(), true) : udmData->prop;
	if(!prop)
		return;
	prop = value;
}
template<typename T>
static bool udm_write_property_vt(UdmProperty udmData, const char *path, UdmType type, T *values, uint32_t numValues)
{
	return udm::visit(static_cast<udm::Type>(type), [&udmData, path, values, numValues](auto tag) {
		using TProperty = typename decltype(tag)::type;
		using TUnderlying = UdmUnderlyingType<TProperty>;
		if constexpr(std::is_same_v<TUnderlying, T>) {
			constexpr auto n = udm::get_numeric_component_count(udm::type_to_enum<TProperty>());
			if(n != numValues)
				return false;
			auto prop = (strlen(path) > 0) ? udmData->prop.Add(path, udm::type_to_enum<TProperty>(), true) : udmData->prop;
			if(!prop)
				return false;
			prop = *reinterpret_cast<TProperty *>(values);
			return true;
		}
		return false;
	});
}
template<typename T>
static T *udm_read_property_vt(UdmProperty udmData, const char *path, UdmType type, uint32_t &outNumValues)
{
	auto childProp = get_property(udmData, path);
	if(!childProp)
		return nullptr;
	if(!childProp.IsArrayItem() && udm::is_array_type(childProp->type)) {
		auto &a = childProp.GetValue<udm::Array>();
		if(!a.IsValueType(static_cast<udm::Type>(type)))
			return nullptr;
		auto n = a.GetSize();
		outNumValues = n;
		auto *r = new T[n];
		memcpy(r, a.GetValuePtr(0), n * sizeof(T));
		udmData->data.AddDeleter([r]() { delete[] r; });
		return r;
	}
	return udm::visit(childProp->type, [&udmData, &childProp, type, &outNumValues](auto tag) -> T * {
		using TTag = typename decltype(tag)::type;
		using TUnderlying = UdmUnderlyingType<TTag>;
		if constexpr(std::is_same_v<TUnderlying, T>) {
			auto val = childProp.ToValue<TTag>();
			if(val.has_value()) {
				auto &v = *val;
				constexpr auto n = udm::get_numeric_component_count(udm::type_to_enum<TTag>());
				static_assert(sizeof(TTag) == n * sizeof(T));
				outNumValues = n;
				auto *r = new T[n];
				memcpy(r, &v, sizeof(v));
				udmData->data.AddDeleter([r]() { delete[] r; });
				return r;
			}
		}
		return nullptr;
	});
}
template<typename T>
static T *udm_read_property_svt(UdmProperty udmData, const char *path, uint32_t arrayIndex, uint32_t memberIndex, UdmType type, uint32_t &outNumValues)
{
	auto childProp = get_property(udmData, path);
	if(!childProp || !udm::is_array_type(childProp.GetType()))
		return nullptr;
	auto &a = childProp.GetValue<udm::Array>();
	auto *strct = a.GetStructuredDataInfo();
	if(!strct || arrayIndex >= a.GetSize() || memberIndex >= strct->GetMemberCount())
		return nullptr;
	uint64_t offset = 0;
	for(auto i = decltype(memberIndex) {0u}; i < memberIndex; ++i)
		offset += udm::size_of(strct->types[i]);

	auto *ptrMemberValue = static_cast<uint8_t *>(a.GetValuePtr(arrayIndex)) + offset;
	auto memberType = strct->types[memberIndex];
	return udm::visit(memberType, [&udmData, &strct, type, memberIndex, ptrMemberValue, &outNumValues](auto tag) -> T * {
		using TMember = typename decltype(tag)::type;
		return udm::visit(static_cast<udm::Type>(type), [&udmData, ptrMemberValue, &outNumValues](auto tag) -> T * {
			using TValue = typename decltype(tag)::type;
			if constexpr(udm::is_convertible<TMember, TValue>() && std::is_same_v<UdmUnderlyingType<TValue>, T>) {
				auto v = udm::convert<TMember, TValue>(*reinterpret_cast<TMember *>(ptrMemberValue));
				constexpr auto n = udm::get_numeric_component_count(udm::type_to_enum<TValue>());
				static_assert(sizeof(TValue) == n * sizeof(T));
				outNumValues = n;
				auto *r = new T[n];
				memcpy(r, &v, sizeof(v));
				udmData->data.AddDeleter([r]() { delete[] r; });
				return r;
			}
			else
				return nullptr;
		});
	});
}
template<typename T>
static bool udm_write_property_svt(UdmProperty udmData, const char *path, uint32_t arrayIndex, uint32_t memberIndex, UdmType type, T *values, uint32_t numValues)
{
	auto childProp = (strlen(path) > 0) ? udmData->prop[path] : udmData->prop;
	if(!childProp || !udm::is_array_type(childProp.GetType()))
		return false;
	auto &a = childProp.GetValue<udm::Array>();
	auto *strct = a.GetStructuredDataInfo();
	if(!strct || arrayIndex >= a.GetSize() || memberIndex >= strct->GetMemberCount())
		return false;
	uint64_t offset = 0;
	for(auto i = decltype(memberIndex) {0u}; i < memberIndex; ++i)
		offset += udm::size_of(strct->types[i]);

	auto *ptrMemberValue = static_cast<uint8_t *>(a.GetValuePtr(arrayIndex)) + offset;
	auto memberType = strct->types[memberIndex];
	return udm::visit(static_cast<udm::Type>(type), [memberType, numValues, ptrMemberValue, values](auto tag) {
		using TT = typename decltype(tag)::type;
		using TUnderlying = UdmUnderlyingType<TT>;
		if(udm::size_of(memberType) != udm::size_of(udm::type_to_enum<TUnderlying>()) * numValues)
			return false;
		memcpy(ptrMemberValue, values, udm::size_of(memberType));
		return true;
	});
}

static udm::StructDescription to_struct_description(uint32_t numMembers, UdmType *types, const char **names)
{
	udm::StructDescription desc {};
	desc.types.resize(numMembers);
	desc.names.resize(numMembers);
	for(auto i = decltype(numMembers) {0u}; i < numMembers; ++i) {
		desc.types[i] = static_cast<udm::Type>(types[i]);
		desc.names[i] = names[i];
	}
	return desc;
}

#define DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(NAME, TYPE)                                                                                                                                                                                                                                            \
	DLLUDM TYPE udm_read_property_##NAME(UdmProperty udmData, const char *path, TYPE defaultValue) { return udm_read_property_t(udmData, path, defaultValue); }                                                                                                                                  \
	DLLUDM void udm_write_property_##NAME(UdmProperty udmData, const char *path, TYPE value) { udm_write_property_t(udmData, path, value); }                                                                                                                                                     \
	DLLUDM TYPE *udm_read_property_sv##NAME(UdmProperty udmData, const char *path, uint32_t arrayIndex, uint32_t memberIndex, UdmType type, uint32_t *outNumValues) { return udm_read_property_svt<TYPE>(udmData, path, arrayIndex, memberIndex, type, *outNumValues); }                         \
	DLLUDM bool udm_write_property_sv##NAME(UdmProperty udmData, const char *path, uint32_t arrayIndex, uint32_t memberIndex, UdmType type, TYPE *values, uint32_t numValues) { return udm_write_property_svt<TYPE>(udmData, path, arrayIndex, memberIndex, type, values, numValues); }          \
	DLLUDM TYPE *udm_read_property_v##NAME(UdmProperty udmData, const char *path, UdmType type, uint32_t *outNumValues) { return udm_read_property_vt<TYPE>(udmData, path, type, *outNumValues); }                                                                                               \
	DLLUDM bool udm_write_property_v##NAME(UdmProperty udmData, const char *path, UdmType type, TYPE *values, uint32_t numValues) { return udm_write_property_vt<TYPE>(udmData, path, type, values, numValues); }                                                                                \
	DLLUDM void udm_destroy_property_v##NAME(TYPE *value) { delete[] value; }

#define LOOKUP_FUNDAMENTAL_ADDRESSES(NAME, TYPE)                                                                                                                                                                                                                                                 \
	auto *udm_read_property_##NAME = lib->FindSymbolAddress<TYPE (*)(UdmProperty, const char *, TYPE)>("udm_read_property_" #NAME);                                                                                                                                                              \
	auto *udm_write_property_##NAME = lib->FindSymbolAddress<void (*)(UdmProperty, const char *, TYPE)>("udm_write_property_" #NAME);                                                                                                                                                            \
	auto *udm_read_property_sv##NAME = lib->FindSymbolAddress<TYPE *(*)(UdmProperty, const char *, uint32_t, uint32_t, UdmType, uint32_t *)>("udm_read_property_sv" #NAME);                                                                                                                      \
	auto *udm_write_property_sv##NAME = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, uint32_t, uint32_t, UdmType, TYPE *, uint32_t)>("udm_write_property_sv" #NAME);                                                                                                               \
	auto *udm_read_property_v##NAME = lib->FindSymbolAddress<TYPE *(*)(UdmProperty, const char *, UdmType, uint32_t *)>("udm_read_property_v" #NAME);                                                                                                                                            \
	auto *udm_destroy_property_v##NAME = lib->FindSymbolAddress<void (*)(TYPE *)>("udm_destroy_property_v" #NAME);
extern "C" {
DLLUDM UdmData udm_load(const char *fileName, bool clearDataOnDestruction)
{
	auto fileMode = filemanager::FileMode::Read;
	std::string ext;
	if(ufile::get_extension(fileName, &ext) && ext.length() > 2 && ext.substr(ext.length() - 2) == "_b")
		fileMode |= filemanager::FileMode::Binary;
	auto f = filemanager::open_system_file(fileName, fileMode);
	if(!f)
		return nullptr;
	auto udmData = udm::Data::Load(f);
	if(!udmData)
		return nullptr;
	return new BaseUdmData {udmData, clearDataOnDestruction};
}
DLLUDM char *udm_read_header(const char *fileName, udm::Version &outVersion)
{
	auto fileMode = filemanager::FileMode::Read;
	std::string ext;
	if(ufile::get_extension(fileName, &ext) && ext.length() > 2 && ext.substr(ext.length() - 2) == "_b")
		fileMode |= filemanager::FileMode::Binary;
	auto f = filemanager::open_system_file(fileName, fileMode);
	if(!f)
		return nullptr;
	auto udmData = udm::Data::Open(f);
	if(!udmData)
		return nullptr;
	outVersion = udmData->GetAssetData().GetAssetVersion();
	return to_cstring(udmData->GetAssetData().GetAssetType());
}
DLLUDM UdmData udm_create(const char *assetType, uint32_t assetVersion, bool clearDataOnDestruction)
{
	auto udmData = udm::Data::Create(assetType, assetVersion);
	if(!udmData)
		return nullptr;
	return new BaseUdmData {udmData, clearDataOnDestruction};
}
DLLUDM void udm_destroy(UdmData udmData) { delete udmData; }
DLLUDM char *udm_get_asset_type(UdmData udmData) { return to_cstring(*udmData, udmData->data->GetAssetType()); }
DLLUDM udm::Version udm_get_asset_version(UdmData udmData) { return udmData->data->GetAssetVersion(); }

DLLUDM bool udm_save_binary(UdmData udmData, const char *fileName)
{
	auto f = filemanager::open_system_file(fileName, filemanager::FileMode::Write | filemanager::FileMode::Binary);
	if(!f)
		return false;
	return (*udmData)->Save(f);
}
DLLUDM bool udm_save_ascii(UdmData udmData, const char *fileName, uint32_t asciiFlags)
{
	auto f = filemanager::open_system_file(fileName, filemanager::FileMode::Write);
	if(!f)
		return false;
	return (*udmData)->SaveAscii(f, static_cast<udm::AsciiSaveFlags>(asciiFlags));
}
DLLUDM bool udm_is_property_valid(UdmProperty prop, const char *path) { return static_cast<bool>(get_property(prop, path)); }
DLLUDM UdmType udm_get_property_type(UdmProperty prop, const char *path)
{
	auto p = get_property(prop, path);
	return umath::to_integral(p.GetType());
}
DLLUDM uint32_t udm_get_property_child_count(UdmProperty udmData, const char *path)
{
	auto childProp = get_property(udmData, path);
	if(!childProp)
		return 0;
	return childProp.GetChildCount();
}
DLLUDM UdmElementIterator udm_create_property_child_name_iterator(UdmProperty prop, const char *path)
{
	auto childProp = get_property(prop, path);
	if(!childProp)
		return nullptr;
	auto baseProp = std::make_unique<udm::LinkedPropertyWrapper>(childProp);
	auto elIt = baseProp->ElIt();
	auto it = elIt.begin();
	auto *i = new BaseUdmElementIterator {std::move(baseProp), prop, elIt, it};
	prop->data.AddDeleter([i]() { delete i; });
	return i;
}
DLLUDM void udm_destroy_property_child_name_iterator(UdmElementIterator it) { delete it; }
DLLUDM const char *udm_fetch_property_child_name(UdmElementIterator iterator)
{
	if(iterator->it == iterator->wrapper.end())
		return nullptr;
	auto &pair = *iterator->it;
	auto *cstr = to_cstring(iterator->prop->data, std::string {pair.key});
	++iterator->it;
	return cstr;
}

DLLUDM UdmProperty udm_get_root_property(UdmData parent)
{
	auto *prop = new BaseProperty {*parent, (*parent)->GetAssetData().GetData()};
	parent->AddDeleter([prop]() { delete prop; });
	return prop;
}
DLLUDM UdmProperty udm_get_property(UdmProperty parent, const char *path)
{
	auto prop = get_property(parent, path);
	if(!prop)
		return nullptr;
	auto *child = new BaseProperty {parent->data, prop};
	parent->data.AddDeleter([child]() { delete child; });
	return child;
}
DLLUDM UdmProperty udm_get_property_i(UdmProperty parent, uint32_t idx)
{
	auto prop = parent->prop[idx];
	if(!prop)
		return nullptr;
	auto *child = new BaseProperty {parent->data, prop};
	parent->data.AddDeleter([child]() { delete child; });
	return child;
}
DLLUDM void udm_destroy_property(UdmProperty prop) { delete prop; }

DLLUDM char *udm_property_to_json(UdmProperty prop)
{
	std::stringstream ss;
	udm::to_json(prop->prop, ss);
	return to_cstring(prop->data, ss.str());
}
DLLUDM void udm_destroy_string(const char *str) { delete[] str; }

DLLUDM char *udm_get_property_name(UdmProperty prop) { return to_cstring(prop->data, prop->prop.propName); }
DLLUDM int32_t udm_get_property_array_index(UdmProperty prop) { return (prop->prop.arrayIndex != std::numeric_limits<decltype(prop->prop.arrayIndex)>::max()) ? prop->prop.arrayIndex : -1; }
DLLUDM char *udm_get_property_path(UdmProperty prop) { return to_cstring(prop->data, prop->prop.GetPath()); }
DLLUDM UdmProperty udm_get_from_property(UdmProperty prop)
{
	if(!prop->prop.prev)
		return nullptr;
	auto *child = new BaseProperty {prop->data, *prop->prop.prev};
	prop->data.AddDeleter([child]() { delete child; });
	return child;
}

DLLUDM bool udm_get_blob_size(UdmProperty udmData, const char *path, uint64_t &outSize)
{
	auto childProp = get_property(udmData, path);
	auto res = childProp->GetBlobData(nullptr, 0ull, &outSize);
	return res == udm::BlobResult::InsufficientSize;
}
DLLUDM udm::BlobResult udm_read_property_blob(UdmProperty prop, const char *path, uint8_t *outData, size_t outDataSize)
{
	auto childProp = get_property(prop, path);
	return childProp->GetBlobData(outData, outDataSize);
}
DLLUDM char *udm_read_property_s(UdmProperty prop, const char *path, const char *defaultValue)
{
	auto childProp = get_property(prop, path);
	if(!childProp)
		return to_cstring(prop->data, defaultValue);
	auto str = childProp.ToValue<std::string>();
	if(!str.has_value())
		return to_cstring(prop->data, defaultValue);
	return to_cstring(prop->data, *str);
}
DLLUDM bool udm_write_property_s(UdmProperty prop, const char *path, const char *value)
{
	auto childProp = (strlen(path) > 0) ? prop->prop.Add(path, udm::Type::String, true) : prop->prop;
	if(!childProp)
		return false;
	childProp = value;
	return true;
}
DLLUDM const char **udm_read_property_vs(UdmProperty prop, const char *path, uint32_t *outNumValues)
{
	auto childProp = get_property(prop, path);
	if(!childProp || childProp.IsArrayItem() || !udm::is_array_type(childProp.GetType()))
		return nullptr;
	auto &a = childProp.GetValue<udm::Array>();
	if(!a.IsValueType(udm::Type::String))
		return nullptr;
	auto n = a.GetSize();
	*outNumValues = n;
	auto *r = new const char *[n];
	for(auto i = decltype(n) {0u}; i < n; ++i)
		r[i] = to_cstring(prop->data, a.GetValue<std::string>(i));
	prop->data.AddDeleter([r, n]() { delete[] r; });
	return r;
}
DLLUDM bool udm_write_property_vs(UdmProperty prop, const char *path, const char **values, uint32_t numValues)
{
	auto childProp = (strlen(path) > 0) ? prop->prop.AddArray(path, numValues, udm::Type::String, udm::ArrayType::Raw, true) : prop->prop;
	if(!childProp || childProp.IsArrayItem() || !udm::is_array_type(childProp.GetType()))
		return false;
	auto &a = childProp.GetValue<udm::Array>();
	if(!a.IsValueType(udm::Type::String))
		return false;
	a.Resize(numValues);
	for(auto i = decltype(numValues) {0u}; i < numValues; ++i)
		a.SetValue(i, values[i]);
	return true;
}
DLLUDM bool udm_read_property_v(UdmProperty udmData, const char *path, void *outData, uint32_t itemSizeInBytes, uint32_t arrayOffset, uint32_t numItems)
{
	auto childProp = get_property(udmData, path);
	if(!childProp || !udm::is_array_type(childProp.GetType()))
		return false;
	auto &a = childProp.GetValue<udm::Array>();
	if(arrayOffset + numItems > a.GetSize())
		return false;
	if(itemSizeInBytes != a.GetValueSize())
		return false;
	auto *p = a.GetValuePtr(arrayOffset);
	memcpy(outData, p, numItems * itemSizeInBytes);
	return true;
}
DLLUDM bool udm_write_property_v(UdmProperty udmData, const char *path, void *inData, uint32_t itemSizeInBytes, uint32_t arrayOffset, uint32_t numItems)
{
	auto childProp = get_property(udmData, path);
	if(!childProp || !udm::is_array_type(childProp.GetType()))
		return false;
	auto &a = childProp.GetValue<udm::Array>();
	if(arrayOffset + numItems > a.GetSize())
		return false;
	if(itemSizeInBytes != a.GetValueSize())
		return false;
	auto *p = a.GetValuePtr(arrayOffset);
	memcpy(p, inData, numItems * itemSizeInBytes);
	return true;
}
DLLUDM bool udm_read_property(UdmProperty udmData, char *path, UdmType type, void *buffer, uint32_t bufferSize)
{
	auto childProp = get_property(udmData, path);
	if(udm::is_array_type(childProp.GetType())) {
		auto &a = childProp.GetValue<udm::Array>();
		if(bufferSize < a.GetSize())
			return false;
		memcpy(buffer, a.GetValuePtr(0), bufferSize);
		return true;
	}
	if(!udm::is_trivial_type(static_cast<udm::Type>(type)))
		return false;
	return udm::visit_ng(static_cast<udm::Type>(type), [&childProp, buffer, bufferSize](auto tag) {
		using T = typename decltype(tag)::type;
		if(bufferSize != sizeof(T))
			return false;
		auto v = childProp.ToValue<T>();
		if(!v.has_value())
			return false;
		memcpy(buffer, &*v, bufferSize);
		return true;
	});
}
DLLUDM bool udm_write_property(UdmProperty udmData, char *path, UdmType type, void *buffer, uint32_t bufferSize)
{
	auto childProp = get_property(udmData, path);
	if(udm::is_array_type(childProp.GetType())) {
		auto &a = childProp.GetValue<udm::Array>();
		if(bufferSize < a.GetSize())
			return false;
		memcpy(a.GetValuePtr(0), buffer, bufferSize);
		return true;
	}
	if(static_cast<udm::Type>(type) == udm::Type::String) {
		auto *str = static_cast<char *>(buffer);
		if(strlen(str) + 1 != bufferSize || str[bufferSize - 1] != '\0')
			return false;
		childProp = str;
		return true;
	}
	if(!udm::is_trivial_type(static_cast<udm::Type>(type)))
		return false;
	return udm::visit_ng(static_cast<udm::Type>(type), [&childProp, buffer, bufferSize](auto tag) {
		using T = typename decltype(tag)::type;
		if(bufferSize != sizeof(T))
			return false;
		childProp = *static_cast<T *>(buffer);
		return true;
	});
}
enum class ReadArrayPropertyResult : uint32_t { Success = 0, NotAnArrayType, RequestedRangeOutOfBounds, BufferSizeDoesNotMatchExpectedSize };
DLLUDM ReadArrayPropertyResult udm_read_array_property(UdmProperty udmData, char *path, UdmType type, void *buffer, uint32_t bufferSize, uint32_t arrayOffset, uint32_t arraySize)
{
	auto childProp = get_property(udmData, path);
	if(!udm::is_array_type(childProp.GetType()))
		return ReadArrayPropertyResult::NotAnArrayType;
	auto &a = childProp.GetValue<udm::Array>();
	if((arrayOffset + arraySize) > a.GetSize() || (!udm::is_trivial_type(static_cast<udm::Type>(type)) && static_cast<udm::Type>(type) != udm::Type::Struct))
		return ReadArrayPropertyResult::RequestedRangeOutOfBounds;
	auto szPerValue = a.GetValueSize();
	if(szPerValue * arraySize != bufferSize)
		return ReadArrayPropertyResult::BufferSizeDoesNotMatchExpectedSize;
	memcpy(buffer, a.GetValuePtr(arrayOffset), arraySize * szPerValue);
	return ReadArrayPropertyResult::Success;
}
DLLUDM bool udm_write_array_property(UdmProperty udmData, char *path, UdmType type, void *buffer, uint32_t bufferSize, uint32_t arrayOffset, uint32_t arraySize, UdmArrayType arrayType, uint32_t numMembers, UdmType *types, const char **names)
{
	if(numMembers > 0) {
		if(static_cast<udm::Type>(type) != udm::Type::Struct)
			return false;
		udm::StructDescription strctDesc {};
		strctDesc.types.reserve(numMembers);
		strctDesc.names.reserve(numMembers);
		for(auto i = decltype(numMembers) {0u}; i < numMembers; ++i) {
			strctDesc.types[i] = static_cast<udm::Type>(types[i]);
			strctDesc.names[i] = names[i];
		}
		if(strctDesc.GetDataSizeRequirement() * arraySize != bufferSize)
			return false;
		udmData->prop.AddArray(path, strctDesc, buffer, numMembers, static_cast<udm::ArrayType>(arrayType));
		return true;
	}
	if(!udm::is_trivial_type(static_cast<udm::Type>(type)))
		return false;
	return udm::visit(static_cast<udm::Type>(type), [udmData, path, arraySize, type, buffer, arrayType, bufferSize](auto tag) {
		using T = typename decltype(tag)::type;
		if(sizeof(T) * arraySize != bufferSize)
			return false;
		udmData->prop.AddArray<T>(path, arraySize, static_cast<T *>(buffer), static_cast<udm::ArrayType>(arrayType));
		return true;
	});
}
DLLUDM size_t udm_size_of_type(UdmType type) { return udm::size_of(static_cast<udm::Type>(type)); }
DLLUDM size_t udm_size_of_struct(uint32_t numMembers, UdmType *types)
{
	size_t sz = 0;
	for(auto i = decltype(numMembers) {0u}; i < numMembers; ++i)
		sz += udm::size_of(static_cast<udm::Type>(types[i]));
	return sz;
}
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(b, bool)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(f, float)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(d, double)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(i, int32_t)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(ui, uint32_t)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(i8, int8_t)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(ui8, uint8_t)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(i16, int16_t)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(ui16, uint16_t)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(i64, int64_t)
DEFINE_FUNDAMENTAL_TYPE_FUNCTIONS(ui64, uint64_t)
DLLUDM bool udm_add_property_struct(UdmProperty udmData, const char *path, uint32_t numMembers, UdmType *types, const char **names)
{
	auto childProp = (strlen(path) > 0) ? udmData->prop.Add(path, udm::Type::Struct, true) : udmData->prop;
	auto *strct = childProp.GetValuePtr<udm::Struct>();
	if(!strct)
		return false;
	strct->SetDescription(to_struct_description(numMembers, types, names));
	return true;
}
DLLUDM bool udm_add_property_array(UdmProperty udmData, const char *path, UdmType type, UdmArrayType arrayType, uint32_t size)
{
	auto childProp = udmData->prop.AddArray(path, size, static_cast<udm::Type>(type), static_cast<udm::ArrayType>(arrayType), true);
	if(!childProp || !udm::is_array_type(childProp->type))
		return false;
	return true;
}
DLLUDM bool udm_add_property_struct_array(UdmProperty udmData, const char *path, uint32_t numMembers, UdmType *types, const char **names, UdmArrayType arrayType, uint32_t size)
{
	auto childProp = udmData->prop.AddArray(path, to_struct_description(numMembers, types, names), size, static_cast<udm::ArrayType>(arrayType), true);
	if(!childProp || !udm::is_array_type(childProp->type))
		return false;
	return true;
}

DLLUDM UdmType udm_get_array_value_type(UdmProperty udmData, const char *path)
{
	auto childProp = get_property(udmData, path);
	if(!childProp || childProp.IsArrayItem() || !udm::is_array_type(childProp->type))
		return umath::to_integral(udm::Type::Invalid);
	return umath::to_integral(childProp.GetValue<udm::Array>().GetValueType());
}
DLLUDM uint32_t udm_get_array_size(UdmProperty udmData, const char *path)
{
	auto childProp = get_property(udmData, path);
	if(!childProp || childProp.IsArrayItem() || !udm::is_array_type(childProp->type))
		return 0;
	return childProp.GetValue<udm::Array>().GetSize();
}
DLLUDM bool udm_set_array_size(UdmProperty udmData, const char *path, uint32_t newSize)
{
	auto childProp = get_property(udmData, path);
	if(!childProp || childProp.IsArrayItem() || !udm::is_array_type(childProp->type))
		return false;
	childProp.GetValue<udm::Array>().Resize(newSize);
	return true;
}
DLLUDM UdmType *udm_get_struct_member_types(UdmProperty udmData, const char *path, uint32_t *outNumMembers)
{
	*outNumMembers = 0;
	auto childProp = get_property(udmData, path);
	if(!childProp || childProp.IsArrayItem() || !udm::is_array_type(childProp->type))
		return nullptr;
	auto &a = childProp.GetValue<udm::Array>();
	auto *strct = a.GetStructuredDataInfo();
	if(!strct)
		return nullptr;
	auto n = strct->GetMemberCount();
	auto *r = new UdmType[n];
	for(auto i = decltype(n) {0u}; i < n; ++i)
		r[i] = umath::to_integral(strct->types[i]);
	*outNumMembers = n;
	udmData->data.AddDeleter([r]() { delete[] r; });
	return r;
}
DLLUDM char **udm_get_struct_member_names(UdmProperty udmData, const char *path, uint32_t *outNumMembers)
{
	*outNumMembers = 0;
	auto childProp = get_property(udmData, path);
	if(!childProp || childProp.IsArrayItem() || !udm::is_array_type(childProp->type))
		return nullptr;
	auto &a = childProp.GetValue<udm::Array>();
	auto *strct = a.GetStructuredDataInfo();
	if(!strct)
		return nullptr;
	auto n = strct->GetMemberCount();
	auto *r = new char *[n];
	for(auto i = decltype(n) {0u}; i < n; ++i)
		r[i] = to_cstring(udmData->data, strct->names[i]);
	*outNumMembers = n;
	udmData->data.AddDeleter([r, n]() { delete[] r; });
	return r;
}

DLLUDM char *udm_property_to_ascii(UdmProperty udmData, const char *propName)
{
	auto *prop = udmData->prop.GetProperty();
	if(!prop)
		return nullptr;
	std::stringstream ss;
	prop->ToAscii(udm::AsciiSaveFlags::DontCompressLz4Arrays, ss, propName);
	return to_cstring(udmData->data, ss.str());
}

DLLUDM void udm_free_memory(UdmData udmData) { udmData->FreeMemory(); }

DLLUDM void udm_add_custom_mount_path(const char *path) { filemanager::add_custom_mount_directory(path, true); }
DLLUDM void udm_pose_to_matrix(const float pos[3], const float rot[4], const float scale[3], float *outMatrix)
{
	umath::ScaledTransform pose {Vector3 {pos[0], pos[1], pos[2]}, Quat {rot[0], rot[1], rot[2], rot[3]}, Vector3 {scale[0], scale[1], scale[2]}};
	auto mat = pose.ToMatrix();
	memcpy(outMatrix, &mat, sizeof(mat));
}
}

void udm::detail::test_c_wrapper()
{
	// Note: This will only work if util_udm was built as a shared library!
	std::string err;
	auto lib = util::Library::Get("util_udm", &err);
	if(!lib)
		return;
	auto *udm_load = lib->FindSymbolAddress<UdmData (*)(const char *, bool)>("udm_load");
	auto *udm_create = lib->FindSymbolAddress<UdmData (*)(const char *, uint32_t, bool)>("udm_create");
	auto *udm_destroy = lib->FindSymbolAddress<void (*)(UdmData)>("udm_destroy");
	auto *udm_save_binary = lib->FindSymbolAddress<bool (*)(UdmData, const char *)>("udm_save_binary");
	auto *udm_save_ascii = lib->FindSymbolAddress<bool (*)(UdmData, const char *, uint32_t)>("udm_save_ascii");
	auto *udm_is_property_valid = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *)>("udm_is_property_valid");
	auto *udm_get_root_property = lib->FindSymbolAddress<UdmProperty (*)(UdmData)>("udm_get_root_property");
	auto *udm_get_property = lib->FindSymbolAddress<UdmProperty (*)(UdmProperty, const char *)>("udm_get_property");
	auto *udm_get_property_i = lib->FindSymbolAddress<UdmProperty (*)(UdmProperty, uint32_t)>("udm_get_property_i");
	auto *udm_destroy_property = lib->FindSymbolAddress<void (*)(UdmProperty)>("udm_destroy_property");
	auto *udm_property_to_json = lib->FindSymbolAddress<char *(*)(UdmProperty)>("udm_property_to_json");
	auto *udm_destroy_string = lib->FindSymbolAddress<void (*)(const char *)>("udm_destroy_string");
	auto *udm_read_property_s = lib->FindSymbolAddress<char *(*)(UdmProperty, const char *, const char *)>("udm_read_property_s");
	auto *udm_write_property_s = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, const char *)>("udm_write_property_s");
	auto *udm_read_property_vs = lib->FindSymbolAddress<char *(*)(UdmProperty, const char *, uint32_t *)>("udm_read_property_vs");
	auto *udm_write_property_vs = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, const char **, uint32_t)>("udm_write_property_vs");
	auto *udm_read_property_v = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, void *, uint32_t, uint32_t, uint32_t)>("udm_read_property_v");
	auto *udm_write_property_v = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, void *, uint32_t, uint32_t, uint32_t)>("udm_write_property_v");
	auto *udm_size_of_type = lib->FindSymbolAddress<size_t (*)(UdmType)>("udm_size_of_type");
	auto *udm_size_of_struct = lib->FindSymbolAddress<size_t (*)(uint32_t, UdmType *)>("udm_size_of_struct");

	auto *udm_free_memory = lib->FindSymbolAddress<void (*)(UdmData)>("udm_free_memory");

	auto *udm_get_property_name = lib->FindSymbolAddress<char *(*)(UdmProperty)>("udm_get_property_name");
	auto *udm_get_property_array_index = lib->FindSymbolAddress<int32_t (*)(UdmProperty)>("udm_get_property_array_index");
	auto *udm_get_property_path = lib->FindSymbolAddress<char *(*)(UdmProperty)>("udm_get_property_path");
	auto *udm_get_from_property = lib->FindSymbolAddress<UdmProperty (*)(UdmProperty)>("udm_get_from_property");

	LOOKUP_FUNDAMENTAL_ADDRESSES(b, bool);
	LOOKUP_FUNDAMENTAL_ADDRESSES(f, float);
	LOOKUP_FUNDAMENTAL_ADDRESSES(d, double);
	LOOKUP_FUNDAMENTAL_ADDRESSES(i, int32_t);
	LOOKUP_FUNDAMENTAL_ADDRESSES(ui, uint32_t);
	LOOKUP_FUNDAMENTAL_ADDRESSES(i8, int8_t);
	LOOKUP_FUNDAMENTAL_ADDRESSES(ui8, uint8_t);
	LOOKUP_FUNDAMENTAL_ADDRESSES(i16, int16_t);
	LOOKUP_FUNDAMENTAL_ADDRESSES(ui16, uint16_t);
	LOOKUP_FUNDAMENTAL_ADDRESSES(i64, int64_t);
	LOOKUP_FUNDAMENTAL_ADDRESSES(ui64, uint64_t);
	auto *udm_add_property_struct = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, uint32_t, UdmType *, const char **)>("udm_add_property_struct");
	auto *udm_add_property_array = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, UdmType, UdmArrayType, uint32_t)>("udm_add_property_array");
	auto *udm_add_property_struct_array = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, uint32_t, UdmType *, const char **, UdmArrayType, uint32_t)>("udm_add_property_struct_array");

	auto *udm_get_array_value_type = lib->FindSymbolAddress<UdmType (*)(UdmProperty, const char *)>("udm_get_array_value_type");
	auto *udm_get_array_size = lib->FindSymbolAddress<uint32_t (*)(UdmProperty, const char *)>("udm_get_array_size");
	auto *udm_set_array_size = lib->FindSymbolAddress<bool (*)(UdmProperty, const char *, uint32_t)>("udm_set_array_size");
	auto *udm_get_struct_member_types = lib->FindSymbolAddress<UdmType *(*)(UdmProperty, const char *, uint32_t *)>("udm_get_struct_member_types");

	auto *udm_get_property_type = lib->FindSymbolAddress<UdmType (*)(UdmProperty, const char *)>("udm_get_property_type");
	auto *udm_get_property_child_count = lib->FindSymbolAddress<uint32_t (*)(UdmProperty, const char *)>("udm_get_property_child_count");
	auto *udm_create_property_child_name_iterator = lib->FindSymbolAddress<UdmElementIterator (*)(UdmProperty, const char *)>("udm_create_property_child_name_iterator");
	auto *udm_destroy_property_child_name_iterator = lib->FindSymbolAddress<void (*)(UdmElementIterator)>("udm_destroy_property_child_name_iterator");
	auto *udm_fetch_property_child_name = lib->FindSymbolAddress<const char *(*)(UdmElementIterator)>("udm_fetch_property_child_name");

	// Example

	// Create new UDM file
	auto *udmData = udm_create("PMDL", 1 /* version */, true /* clearDataOnDestruction */);
	if(udmData) {
		auto *root = udm_get_root_property(udmData);
		udm_write_property_f(root, "mass", 12.f); // Write float property
		// Add an uncompressed array
		udm_add_property_array(root, "hitboxes", umath::to_integral(udm::Type::Element), umath::to_integral(udm::ArrayType::Raw), 2 /* arraySize */);

		auto hb0 = udm_get_property(root, "hitboxes[0]");
		udm_write_property_s(hb0, "hitGroup", "LeftLeg"); // Write string: hitboxes[0]/hitGroup = "LeftLeg"
		std::array<float, 3> min = {-10.f, -3.f, -5.f};
		std::array<float, 3> max = {3.f, 12.f, 55.f};
		// Write vector properties
		udm_write_property_vf(hb0, "bounds/min", umath::to_integral(udm::Type::Vector3), min.data(), min.size());
		udm_write_property_vf(hb0, "bounds/max", umath::to_integral(udm::Type::Vector3), max.data(), max.size());

		udm_write_property_i(hb0, "bone", 3);

		auto hitboxes = udm_get_property(root, "hitboxes");
		auto hb1 = udm_get_property_i(hitboxes, 1 /* arrayIndex */); // Alternative array indexing method
		udm_write_property_s(hb1, "hitGroup", "RightLeg");           // Write string: hitboxes[1]/hitGroup = "RightLeg"

		udm_write_property_i(hb1, "bone", 12);

		// Create a struct with three members: position (vec3), normal (vec3), uv (vec2)
		std::array<uint8_t, 3> types = {umath::to_integral(udm::Type::Vector3), umath::to_integral(udm::Type::Vector3), umath::to_integral(udm::Type::Vector2)};
		std::array<const char *, 3> names = {"pos", "n", "uv"}; // Member names
		udm_add_property_struct_array(root, "vertices", types.size() /* memberCount */, types.data(), names.data(), umath::to_integral(udm::ArrayType::Compressed), 3 /* arraySize */);

		auto vertices = udm_get_property(root, "vertices");
		std::array<float, 3> pos0 = {-1.f, 5.f, 12.f};
		std::array<float, 3> pos1 = {6.f, 5.4f, 12.5f};
		std::array<float, 3> pos2 = {-5.f, 33.3f, 43.f};

		std::array<float, 3> n = {0.f, 1.f, 0.f};
		std::array<float, 2> uv = {1.f, 0.f};

		// Write first vertex
		udm_write_property_svf(vertices, "", 0 /* arrayIndex */, 0 /* memberIndex */, umath::to_integral(udm::Type::Vector3), pos0.data(), pos0.size());
		udm_write_property_svf(root, "vertices", 0 /* arrayIndex */, 1 /* memberIndex */, umath::to_integral(udm::Type::Vector3), n.data(), n.size());
		udm_write_property_svf(root, "vertices", 0 /* arrayIndex */, 2 /* memberIndex */, umath::to_integral(udm::Type::Vector2), uv.data(), uv.size());

		// Write second vertex
		udm_write_property_svf(root, "vertices", 1 /* arrayIndex */, 0 /* memberIndex */, umath::to_integral(udm::Type::Vector3), pos1.data(), 3);
		udm_write_property_svf(root, "vertices", 1 /* arrayIndex */, 1 /* memberIndex */, umath::to_integral(udm::Type::Vector3), n.data(), n.size());
		udm_write_property_svf(root, "vertices", 1 /* arrayIndex */, 2 /* memberIndex */, umath::to_integral(udm::Type::Vector2), uv.data(), uv.size());

		// Write third vertex
		udm_write_property_svf(root, "vertices", 2 /* arrayIndex */, 0 /* memberIndex */, umath::to_integral(udm::Type::Vector3), pos2.data(), 3);
		udm_write_property_svf(root, "vertices", 2 /* arrayIndex */, 1 /* memberIndex */, umath::to_integral(udm::Type::Vector3), n.data(), n.size());
		udm_write_property_svf(root, "vertices", 2 /* arrayIndex */, 2 /* memberIndex */, umath::to_integral(udm::Type::Vector2), uv.data(), uv.size());

		auto path = filemanager::get_program_write_path();

		// Save ascii version
		udm_save_ascii(udmData, (path + "/udm_example_file.pmdl").c_str(), umath::to_integral(udm::AsciiSaveFlags::IncludeHeader | udm::AsciiSaveFlags::DontCompressLz4Arrays));

		// Save binary version
		udm_save_binary(udmData, (path + "/udm_example_file.pmdl_b").c_str());

		// Also print all data as ascii to console
		auto *str = udm_property_to_ascii(root, "root");
		std::cout << "Data in ascii format:\n" << str << std::endl;

		// Destroy all data
		udm_destroy(udmData);
	}

	auto path = filemanager::get_program_write_path();
	// Load binary file
	udmData = udm_load((path + "/udm_example_file.pmdl").c_str(), true /* clearDataOnDestruction */);
	if(udmData != nullptr) {
		auto *root = udm_get_root_property(udmData);

		// Read the data we've written above
		auto mass = udm_read_property_f(root, "mass", 1.f /* default */);
		std::cout << "Mass: " << mass << std::endl;

		auto boneIdx = udm_read_property_ui(root, "hitboxes[0]/bone", 0 /* default */); // Bone index of first hitbox
		std::cout << "Bone index of first hitbox: " << boneIdx << std::endl;

		auto hitboxes = udm_get_property(root, "hitboxes");
		auto hb1 = udm_get_property_i(hitboxes, 1 /* arrayIndex */); // Alternative to above method
		auto *hitGroup = udm_read_property_s(hb1, "hitGroup", "");
		std::cout << "HitGroup of hitbox 1: " << hitGroup << std::endl;

		auto propBoundsMin = udm_get_property(root, "hitboxes[0]/bounds/min");
		uint32_t numItems = 0;
		auto *boundsMin = udm_read_property_vf(propBoundsMin, "", umath::to_integral(udm::Type::Float), &numItems);

		std::array<uint8_t, 3> types = {umath::to_integral(udm::Type::Vector3), umath::to_integral(udm::Type::Vector3), umath::to_integral(udm::Type::Vector2)};
		std::array<const char *, 3> names = {"pos", "n", "uv"};
		uint32_t numValues = 0;
		auto *pos0 = udm_read_property_svf(root, "vertices", 0 /* arrayIndex */, 0 /* memberIndex */, umath::to_integral(udm::Type::Vector3), &numValues);
		assert(numValues == 3);
		std::cout << "Position of vertex 0: " << pos0[0] << "," << pos0[1] << "," << pos0[2] << std::endl;

		udm_destroy(udmData);
	}
}
