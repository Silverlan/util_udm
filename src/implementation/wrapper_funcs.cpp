// SPDX-FileCopyrightText: © 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;


#include "mathutil/glmutil.h"

module pragma.udm;

import :property;
import :wrapper_funcs;

// Utility functions to prevent circular dependencies
template<typename T>
	udm::PProperty udm::create_property(T &&value) {
	return Property::Create<T>(std::forward<T>(value));
}
udm::PProperty udm::copy_property(const Property &prop) {return std::make_shared<Property>(prop);}
udm::Type udm::get_property_type(const Property &prop) {return prop.type;}

udm::Type udm::get_property_type(const PropertyWrapper &prop) {return prop->type;}
bool udm::is_property_type(const Property &prop, Type type) {return prop.IsType(type);}
udm::DataValue udm::get_property_value(Property &prop) {return prop.value;}
udm::DataValue udm::get_property_value(PropertyWrapper &prop) {return prop->value;}
template<typename T>
	T &udm::get_property_value(Property &prop) {return prop.GetValue<T>();}
template<typename T>
	T &udm::get_property_value(PropertyWrapper &prop) {return prop->GetValue<T>();}
template<typename T>
	T *udm::get_property_value_ptr(Property &prop) {return prop.GetValuePtr<T>();}
template<typename T>
	std::optional<T> udm::to_property_value(Property &prop) {return prop.ToValue<T>();}
template<typename T> requires(!is_struct_type<T>)
	void udm::set_property_value(Property &prop, T &&value) {prop = std::forward<T>(value);}
template<class T>
	udm::BlobResult udm::get_property_blob_data(Property &prop, T &v) {return prop.GetBlobData<T>(v);}

udm::Type udm::get_array_value_type(const Array &a) {return a.GetValueType();}
uint16_t udm::get_array_structured_data_info_data_size_requirement(Array &a)
{
	return a.GetStructuredDataInfo()->GetDataSizeRequirement();
}
template<typename T>
	T &udm::get_array_value(Array &a, uint32_t idx) {return a.GetValue<T>(idx);}
template<typename T>
	T *udm::get_array_value_ptr(Array &a, uint32_t idx) {return a.GetValuePtr<T>(idx);}
template<typename T> requires(!is_struct_type<T>)
	void udm::set_array_value(Array &a, uint32_t idx, T &&v) {a.SetValue<T>(idx, std::forward<T>(v));}
uint32_t udm::get_array_value_size(const Array &a) {return a.GetValueSize();}
uint32_t udm::get_array_size(const Array &a) {return a.GetSize();}
void *udm::get_array_values(Array &a) {return a.GetValues();}
bool udm::is_array_value_type(const Array &a, Type pvalueType) {return a.IsValueType(pvalueType);}

template<typename T>
	void udm::get_array_begin_iterator(Array &a, ArrayIterator<T> &outIt) {outIt = a.begin<T>();}
template<typename T>
	void udm::get_array_end_iterator(Array &a, ArrayIterator<T> &outIt) {outIt = a.end<T>();}

udm::PProperty *udm::find_element_child(Element &e, const std::string_view &key) {
	auto it = e.children.find(key);
	if (it == e.children.end())
		return nullptr;
	return &it->second;
}
void udm::remove_element_child(Element &e, const std::string_view &key) {
	auto it = e.children.find(key);
	if (it == e.children.end())
		return;
	e.children.erase(it);
}
void udm::erase_element_child(Element &e, Element &child) {
	e.EraseValue(child);
}
void udm::set_element_child_value(Element &e, const std::string_view &key, const PProperty &prop)
{
	e.children[std::string{key}] = prop;
}
template<typename T>
	void udm::set_element_value(Element &child, T &&v) {
	child.SetValue(child, std::forward<T>(v));
}
udm::PropertyWrapper &udm::get_element_parent_property(Element &e) {
	return e.parentProperty;
}

void udm::set_struct_value(Struct &strct, const void *inData, size_t inSize) {
	strct.Assign(inData, inSize);
}

// The code below will force-instantiate the template functions for all UDM types
namespace udm::impl {
	// export to prevent the function from being optimized away
#ifdef __linux__
__attribute__((visibility("default")))
#else
__declspec(dllexport)
#endif
	void instantiate()
	{
		udm::visit(udm::Type{}, [](auto tag) {
			using T = typename decltype(tag)::type;
			auto &v = *static_cast<T*>(nullptr);

			udm::set_array_value<T>(*static_cast<udm::Array*>(nullptr), 0, std::forward<T>(v));
			udm::set_array_value<T &>(*static_cast<udm::Array*>(nullptr), 0, std::forward<T &>(v));
			udm::set_array_value<const T &>(*static_cast<udm::Array*>(nullptr), 0, std::forward<const T &>(v));

			udm::create_property<T>(std::forward<T>(v));

			if constexpr(
				!std::is_same_v<T, udm::Boolean> && !std::is_same_v<T, udm::Blob> && !std::is_same_v<T, udm::BlobLz4> &&
				!std::is_same_v<T, udm::UInt8>
			) {
				std::vector<T> &vec = *static_cast<std::vector<T>*>(nullptr);
				udm::create_property<std::vector<T>>({});
				udm::create_property<std::vector<T>&>(vec);
				udm::create_property<const std::vector<T>&>(vec);

				udm::set_property_value<std::vector<T>>(*static_cast<udm::Property*>(nullptr), {});
				udm::set_property_value<std::vector<T>&>(*static_cast<udm::Property*>(nullptr), vec);
				udm::set_property_value<const std::vector<T>&>(*static_cast<udm::Property*>(nullptr), vec);
			}

			if constexpr(
				!std::is_same_v<T, udm::Boolean> && !std::is_same_v<T, udm::Array> && !std::is_same_v<T, udm::ArrayLz4> && !std::is_same_v<T, udm::Element>
			) {
				udm::to_property_value<std::vector<T>>(*static_cast<udm::Property*>(nullptr));

				std::vector<T> blobData;
				udm::get_property_blob_data(*static_cast<udm::Property*>(nullptr), blobData);
			}

			if constexpr(!std::is_same_v<T, udm::Boolean> && !std::is_same_v<T, udm::UInt8>) {
				std::vector<T> &vec = *static_cast<std::vector<T>*>(nullptr);
				udm::set_element_value<std::vector<T>>(*static_cast<udm::Element*>(nullptr), {});
				udm::set_element_value<std::vector<T>&>(*static_cast<udm::Element*>(nullptr), vec);
				udm::set_element_value<const std::vector<T>&>(*static_cast<udm::Element*>(nullptr), vec);
			}

			if constexpr(!std::is_same_v<T, udm::UInt8> && !std::is_same_v<T, udm::Array> && !std::is_same_v<T, udm::ArrayLz4> && !std::is_same_v<T, udm::Element>) {
				std::vector<T> &vec = *static_cast<std::vector<T>*>(nullptr);
				udm::set_array_value<std::vector<T>>(*static_cast<udm::Array*>(nullptr), 0u, {});
				udm::set_array_value<std::vector<T>&>(*static_cast<udm::Array*>(nullptr), 0u, vec);
				udm::set_array_value<const std::vector<T>&>(*static_cast<udm::Array*>(nullptr), 0u, vec);
			}

			udm::get_property_value<T>(*static_cast<udm::Property*>(nullptr));
			udm::get_property_value<T>(*static_cast<udm::PropertyWrapper*>(nullptr));
			udm::get_property_value_ptr<T>(*static_cast<udm::Property*>(nullptr));
			udm::to_property_value<T>(*static_cast<udm::Property*>(nullptr));
			udm::set_property_value(*static_cast<udm::Property*>(nullptr), v);

			udm::get_array_value<T>(*static_cast<udm::Array*>(nullptr), 0u);
			udm::get_array_value_ptr<T>(*static_cast<udm::Array*>(nullptr), 0u);
			udm::set_array_value<T>(*static_cast<udm::Array*>(nullptr), 0u, std::forward<T>(v));
			udm::get_array_begin_iterator<T>(*static_cast<udm::Array*>(nullptr), *static_cast<udm::ArrayIterator<T>*>(nullptr));
			udm::get_array_end_iterator<T>(*static_cast<udm::Array*>(nullptr), *static_cast<udm::ArrayIterator<T>*>(nullptr));

			udm::set_element_value<T>(*static_cast<udm::Element*>(nullptr), std::forward<T>(v));

			udm::set_element_value<const T &>(*static_cast<udm::Element*>(nullptr), std::forward<const T &>(v));
			udm::set_element_value<T &>(*static_cast<udm::Element*>(nullptr), std::forward<T &>(v));
			udm::set_property_value<const T &>(*static_cast<udm::Property*>(nullptr), std::forward<const T &>(v));
		});
		udm::get_array_begin_iterator<udm::LinkedPropertyWrapper>(*static_cast<udm::Array*>(nullptr), *static_cast<udm::ArrayIterator<udm::LinkedPropertyWrapper>*>(nullptr));
		udm::get_array_end_iterator<udm::LinkedPropertyWrapper>(*static_cast<udm::Array*>(nullptr), *static_cast<udm::ArrayIterator<udm::LinkedPropertyWrapper>*>(nullptr));

		udm::create_property<std::string_view>({});
		udm::set_property_value<std::string_view>(*static_cast<udm::Property*>(nullptr), {});
		udm::set_array_value<std::string_view>(*static_cast<udm::Array*>(nullptr), 0u,{});
		udm::set_element_value<std::string_view>(*static_cast<udm::Element*>(nullptr), {});

		auto &prop = *static_cast<udm::PProperty*>(nullptr);
		udm::set_property_value<udm::PProperty>(*static_cast<udm::Property*>(nullptr), {});
		udm::set_property_value<udm::PProperty&>(*static_cast<udm::Property*>(nullptr), prop);
		udm::set_property_value<const udm::PProperty&>(*static_cast<udm::Property*>(nullptr), prop);
	}
}
