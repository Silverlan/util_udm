// SPDX-FileCopyrightText: © 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;


export module pragma.udm:wrapper_funcs;

export import :basic_types;
export import :types;

export namespace udm {
	// Utility functions to prevent circular dependencies
	template<typename T>
		PProperty create_property(T &&value);
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

	template<typename T>
		void set_property_value(Property &prop, T &&value);
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

	template<typename T>
		void set_array_value(Array &a, uint32_t idx, T &&v);
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
	template<std::size_t N>
	void set_element_value(Element &child, const char (&v)[N])
	{
		set_element_value(child, std::string_view(v));
	}

	PropertyWrapper &get_element_parent_property(Element &e);

	void set_struct_value(Struct &strct, const void *inData, size_t inSize);
}
