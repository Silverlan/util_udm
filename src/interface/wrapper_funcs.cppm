// SPDX-FileCopyrightText: © 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

export module pragma.udm:wrapper_funcs;

export import :basic_types;
export import :enums;
export import :trivial_types;
export import pragma.util;

export namespace udm {
	// Utility functions to prevent circular dependencies
	// It would be better to simply put the definitions for these functions into
	// a separate partition, but that currently causes unresolved external symbol errors with
	// msvc.
	// TODO: Once the msvc compiler issues are fixed, these functions can be removed and replaced with the
	// underlying function calls.
	template<typename T>
	PProperty create_property(T &&value);
	template<const char *>
	PProperty create_property(const char *v)
	{
		return create_property(std::string_view(v));
	}
	template<std::size_t N>
	PProperty create_property(const char (&v)[N])
	{
		return create_property(std::string_view(v));
	}
	PProperty copy_property(const Property &prop);
	Type get_property_type(const Property &prop);
	Type get_property_type(const PropertyWrapper &prop);
	bool is_property_type(const Property &prop, Type type);
	DataValue get_property_value(Property &prop);
	DataValue get_property_value(PropertyWrapper &prop);
	template<typename T>
	T &get_property_value(Property &prop);
	template<typename T>
	T &get_property_value(PropertyWrapper &prop);
	template<typename T>
	T *get_property_value_ptr(Property &prop);
	template<typename T>
	std::optional<T> to_property_value(Property &prop);

	// Any struct or class that isn't a UDM type is assumed to be a struct type
	template<typename T>
	concept is_struct_type = std::is_class_v<T> && type_to_enum_s<T>() == udm::Type::Invalid && !util::is_specialization<T, std::vector>::value && !util::is_specialization<T, std::shared_ptr>::value && !util::is_specialization_array<T>::value;

	template<typename T>
	    requires(!is_struct_type<T>)
	void set_property_value(Property &prop, T &&value);
	template<typename T>
	    requires(is_struct_type<T>)
	void set_property_value(Property &prop, T &value)
	{
		// Not supported(?)
	}
	template<const char *>
	void set_property_value(Property &prop, const char *v)
	{
		set_property_value(prop, std::string_view(v));
	}
	template<std::size_t N>
	void set_property_value(Property &prop, const char (&v)[N])
	{
		set_property_value(prop, std::string_view(v));
	}

	template<class T>
	BlobResult get_property_blob_data(Property &prop, T &v);

	Type get_array_value_type(const Array &a);
	template<typename T>
	T &get_array_value(Array &a, uint32_t idx);
	template<typename T>
	T *get_array_value_ptr(Array &a, uint32_t idx);

	uint16_t get_array_structured_data_info_data_size_requirement(Array &a);
	// Since we can't know all struct types ahead of time, we handle them separately
	template<typename T>
	    requires(!is_struct_type<T>)
	void set_array_value(Array &a, uint32_t idx, T &&v);
	template<const char *>
	void set_array_value(Array &a, uint32_t idx, const char *v)
	{
		set_array_value(a, idx, std::string_view(v));
	}
	template<std::size_t N>
	void set_array_value(Array &a, uint32_t idx, const char (&v)[N])
	{
		set_array_value(a, idx, std::string_view(v));
	}

	uint32_t get_array_value_size(const Array &a);
	uint32_t get_array_size(const Array &a);
	void *get_array_values(Array &a);
	bool is_array_value_type(const Array &a, Type pvalueType);

	template<typename T>
	class ArrayIterator;
	template<typename T>
	void get_array_begin_iterator(Array &a, ArrayIterator<T> &outIt);
	template<typename T>
	void get_array_end_iterator(Array &a, ArrayIterator<T> &outIt);

	PProperty *find_element_child(Element &e, const std::string_view &key);
	void remove_element_child(Element &e, const std::string_view &key);
	void erase_element_child(Element &e, Element &child);
	void set_element_child_value(Element &e, const std::string_view &key, const PProperty &prop);

	template<typename T>
	void set_element_value(Element &child, T &&v);
	template<const char *>
	void set_element_value(Element &child, const char *v)
	{
		set_element_value(child, std::string_view(v));
	}
	template<std::size_t N>
	void set_element_value(Element &child, const char (&v)[N])
	{
		set_element_value(child, std::string_view(v));
	}

	PropertyWrapper &get_element_parent_property(Element &e);

	void set_struct_value(Struct &strct, const void *inData, size_t inSize);

	template<typename T>
	    requires(is_struct_type<T>)
	void set_array_value(Array &a, uint32_t idx, T &v)
	{
		using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
		auto valueType = get_array_value_type(a);
		if(valueType == Type::Struct) {
			if constexpr(!std::is_fundamental_v<std::remove_extent_t<TBase>>) {
				auto sz = get_array_structured_data_info_data_size_requirement(a);
				if(sizeof(T) != sz)
					throw LogicError {"Attempted to assign data of size " + std::to_string(sizeof(T)) + " to struct of size " + std::to_string(sz) + "!"};
				if constexpr(std::is_rvalue_reference_v<T>)
					static_cast<TBase *>(get_array_values(a))[idx] = std::move(v);
				else
					static_cast<TBase *>(get_array_values(a))[idx] = v;
			}
			else
				throw LogicError {"Attempted to assign fundamental type '" + std::string {typeid(T).name()} + "' to struct, this is not allowed!"};
			return;
		}
	}
}
