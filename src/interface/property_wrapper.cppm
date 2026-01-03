// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.udm:property_wrapper;

import :conversion;
export import :enums;
export import :trivial_types;
export import :types;
import :wrapper_funcs;
import pragma.util;

export {
	namespace udm {
		template<typename T>
		class ArrayIterator;
		struct DLLUDM PropertyWrapper {
			static constexpr std::uint32_t layout_version = 1; // Increment this whenever members of this class are changed

			PropertyWrapper() = default;
			explicit PropertyWrapper(Property &o);
			PropertyWrapper(const PropertyWrapper &other);
			PropertyWrapper(Array &array, uint32_t idx);
			template<typename T>
			void operator=(T &&v) const;
			void operator=(const PropertyWrapper &other);
			void operator=(PropertyWrapper &other);
			void operator=(PropertyWrapper &&other);
			void operator=(const LinkedPropertyWrapper &other);
			void operator=(LinkedPropertyWrapper &other);
			void operator=(Property &other);
			//template<typename T>
			//	operator T() const;
			LinkedPropertyWrapper Add(const std::string_view &path, Type type = Type::Element, bool pathToElements = false) const;
			LinkedPropertyWrapper AddArray(const std::string_view &path, std::optional<uint32_t> size = {}, Type type = Type::Element, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false) const;
			LinkedPropertyWrapper AddArray(const std::string_view &path, StructDescription &&strct, std::optional<uint32_t> size = {}, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false) const;
			LinkedPropertyWrapper AddArray(const std::string_view &path, const StructDescription &strct, std::optional<uint32_t> size = {}, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false) const;
			template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path, const StructDescription &strct, const T *data, uint32_t strctItems, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false) const;
			template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path, const StructDescription &strct, const std::vector<T> &values, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false) const;
			template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path, const std::vector<T> &values, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false) const;
			template<typename T>
			LinkedPropertyWrapper AddArray(const std::string_view &path, uint32_t size, const T *data, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false) const;
			bool IsArrayItem() const;
			bool IsType(Type type) const;
			Type GetType() const;
			Hash CalcHash() const;
			void Merge(const PropertyWrapper &other, MergeFlags mergeFlags = MergeFlags::OverwriteExisting) const;

			Array *GetOwningArray() const;
			BlobResult GetBlobData(void *outBuffer, size_t bufferSize, uint64_t *optOutRequiredSize = nullptr) const;
			BlobResult GetBlobData(void *outBuffer, size_t bufferSize, Type type, uint64_t *optOutRequiredSize = nullptr) const;
			Blob GetBlobData(Type &outType) const;
			template<class T>
			BlobResult GetBlobData(T &v) const;
			template<typename T>
			T &GetValue() const;
			template<typename T>
			T *GetValuePtr() const;
			void *GetValuePtr(Type &outType) const;
			template<typename T>
			T ToValue(const T &defaultValue, bool *optOutIsDefined = nullptr) const;
			template<typename T>
			std::optional<T> ToValue() const;
			template<typename T>
			bool operator>>(T &valOut) const
			{
				return (*this)(valOut);
			}
			template<typename T>
			T operator()(const T &defaultValue) const
			{
				return ToValue<T>(defaultValue);
			}
			template<typename T>
			T operator()(const T &defaultValue, bool &outIsDefined) const
			{
				return ToValue<T>(defaultValue, &outIsDefined);
			}
			template<typename T>
			bool operator()(T &valOut) const
			{
				using TBase = std::remove_reference_t<T>;
				if constexpr(pragma::util::is_specialization<TBase, std::optional>::value) {
					typename TBase::value_type v;
					if(!(*this)(v)) {
						valOut = {};
						return false;
					}
					valOut = v;
					return true;
				}
				else if constexpr(pragma::util::is_specialization<TBase, std::vector>::value || pragma::util::is_specialization<TBase, std::map>::value || pragma::util::is_specialization<TBase, std::unordered_map>::value) {
					bool isDefined;
					valOut = std::move((*this)(const_cast<const T &>(valOut), isDefined));
					return isDefined;
				}
				else if constexpr(std::is_enum_v<TBase>) {
					using TEnum = TBase;
					auto *ptr = GetValuePtr<std::string>();
					if(ptr) {
						auto e = magic_enum::enum_cast<TEnum>(*ptr);
						if(!e.has_value())
							return false;
						valOut = *e;
						return true;
					}
					bool isDefined;
					valOut = static_cast<TEnum>((*this)(reinterpret_cast<const std::underlying_type_t<TEnum> &>(valOut), isDefined));
					return isDefined;
				}
				else if constexpr(std::is_same_v<TBase, PProperty>) {
					if(!prop)
						return false;
					valOut->Assign<false>(*prop);
					return true;
				}
				else if constexpr(std::is_same_v<TBase, Property>) {
					if(!prop)
						return false;
					valOut.Assign<false>(*prop);
					return true;
				}
				else {
					auto *ptr = GetValuePtr<T>();
					if(ptr) {
						valOut = *ptr;
						return true;
					}
					bool isDefined;
					valOut = (*this)(const_cast<const T &>(valOut), isDefined);
					return isDefined;
				}
			}

			// For array properties
			uint32_t GetSize() const;
			void Resize(uint32_t size) const;
			template<typename T>
			ArrayIterator<T> begin() const;
			template<typename T>
			ArrayIterator<T> end() const;
			ArrayIterator<LinkedPropertyWrapper> begin() const;
			ArrayIterator<LinkedPropertyWrapper> end() const;
			LinkedPropertyWrapper operator[](uint32_t idx) const;
			LinkedPropertyWrapper operator[](int32_t idx) const;
			LinkedPropertyWrapper operator[](size_t idx) const;
			//

			// For element properties
			ElementIterator begin_el() const;
			ElementIterator end_el() const;
			uint32_t GetChildCount() const;
			//

			LinkedPropertyWrapper GetFromPath(const std::string_view &key) const;
			LinkedPropertyWrapper operator[](const std::string_view &key) const;
			LinkedPropertyWrapper operator[](const std::string &key) const;
			LinkedPropertyWrapper operator[](const char *key) const;
			bool operator==(const PropertyWrapper &other) const;
			bool operator!=(const PropertyWrapper &other) const;
			template<typename T>
			bool operator==(const T &other) const;
			template<typename T>
			bool operator!=(const T &other) const
			{
				return !operator==(other);
			}
			Property &operator*() const;
			Property *operator->() const;
			operator bool() const;
			Property *prop = nullptr;
			uint32_t arrayIndex = std::numeric_limits<uint32_t>::max();

			LinkedPropertyWrapper *GetLinked();
			const LinkedPropertyWrapper *GetLinked() const { return const_cast<PropertyWrapper *>(this)->GetLinked(); };
		  protected:
			bool IsArrayItem(bool includeIfElementOfArrayItem) const;
			bool linked = false;
		};

		struct DLLUDM LinkedPropertyWrapper : public PropertyWrapper {
			static constexpr std::uint32_t layout_version = 1; // Increment this whenever members of this class are changed

			LinkedPropertyWrapper() : PropertyWrapper {} { linked = true; }
			LinkedPropertyWrapper(const LinkedPropertyWrapper &other);
			LinkedPropertyWrapper(const PropertyWrapper &other) : PropertyWrapper {other} { linked = true; }
			LinkedPropertyWrapper(Property &o) : PropertyWrapper {o} { linked = true; }
			LinkedPropertyWrapper(Array &array, uint32_t idx) : PropertyWrapper {array, idx} { linked = true; }
			bool operator==(const LinkedPropertyWrapper &other) const;
			bool operator!=(const LinkedPropertyWrapper &other) const;
			using PropertyWrapper::operator==;
			using PropertyWrapper::operator!=;
			template<typename T>
			void operator=(T &&v) const;
			void operator=(PropertyWrapper &&v);
			void operator=(LinkedPropertyWrapper &&v);
			void operator=(const PropertyWrapper &v);
			void operator=(const LinkedPropertyWrapper &v);

			// Alias
			template<typename T>
			void operator<<(T &&v)
			{
				operator=(v);
			}

			LinkedPropertyWrapper operator[](uint32_t idx) const;
			LinkedPropertyWrapper operator[](int32_t idx) const;
			LinkedPropertyWrapper operator[](size_t idx) const;

			LinkedPropertyWrapper operator[](const std::string_view &key) const;
			LinkedPropertyWrapper operator[](const std::string &key) const;
			LinkedPropertyWrapper operator[](const char *key) const;

			std::string GetPath() const;
			PProperty ClaimOwnership() const;
			ElementIteratorWrapper ElIt();
			std::unique_ptr<LinkedPropertyWrapper> prev = nullptr;
			std::string propName;

			// For internal use only!
			void InitializeProperty(Type type = Type::Element, bool getOnly = false);
			Property *GetProperty(std::vector<uint32_t> *optOutArrayIndices = nullptr) const;
		};

		template<typename TEnum>
		TEnum string_to_enum(LinkedPropertyWrapperArg udmEnum, TEnum def)
		{
			std::string str;
			udmEnum(str);
			auto e = magic_enum::enum_cast<TEnum>(str);
			return e.has_value() ? *e : def;
		}

		template<typename TEnum>
		TEnum string_to_flags(LinkedPropertyWrapperArg udmEnum, TEnum def)
		{
			std::string str;
			udmEnum(str);
			auto e = magic_enum::enum_flags_cast<TEnum>(str);
			return e.has_value() ? *e : def;
		}

		template<typename TEnum>
		void to_enum_value(LinkedPropertyWrapperArg udmEnum, TEnum &def)
		{
			std::string str;
			udmEnum(str);
			auto e = magic_enum::enum_cast<TEnum>(str);
			def = e.has_value() ? *e : def;
		}

		template<typename TEnum>
		void to_flags(LinkedPropertyWrapperArg udmEnum, TEnum &def)
		{
			std::string str;
			udmEnum(str);
			auto e = magic_enum::enum_flags_cast<TEnum>(str);
			def = e.has_value() ? *e : def;
		}

		template<typename TEnum>
		void write_flag(LinkedPropertyWrapperArg udm, TEnum flags, TEnum flag, const std::string_view &name)
		{
			if(pragma::math::is_flag_set(flags, flag) == false)
				return;
			udm[name] = true;
		}
		template<typename TEnum>
		void read_flag(LinkedPropertyWrapperArg udm, TEnum &flags, TEnum flag, const std::string_view &name)
		{
			if(!udm)
				return;
			pragma::math::set_flag(flags, flag, udm[name](false));
		}
	}

	namespace udm {
		template<typename T>
		void LinkedPropertyWrapper::operator=(T &&v) const
		{
			using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
			if(prop == nullptr) {
				if constexpr(pragma::util::is_specialization<TBase, std::optional>::value) {
					if(!v) // nullopt
						return;
				}
				const_cast<LinkedPropertyWrapper *>(this)->InitializeProperty();
			}
			/*if(prev && prev->arrayIndex != std::numeric_limits<uint32_t>::max() && prev->prev && prev->prev->prop && prev->prev->prop->type == Type::Array)
			{
				(*static_cast<Array*>(prev->prev->prop->value))[prev->arrayIndex][propName] = v;
				return;
			}*/
			PropertyWrapper::operator=(v);
		}

		template<typename T>
		void PropertyWrapper::operator=(T &&v) const
		{
			if(prop == nullptr)
				throw LogicError {"Cannot assign property value: Property is invalid!"};
			using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr(pragma::util::is_specialization<TBase, std::optional>::value) {
				// Value is std::optional
				if(!v) {
					// nullopt case
					// TODO: This code is somewhat redundant (see other cases below) and should be streamlined
					if(is_array_type(get_property_type(*prop))) {
						if(arrayIndex == std::numeric_limits<uint32_t>::max())
							throw LogicError {"Cannot assign propety value to array: No index has been specified!"};
						if(linked && !static_cast<const LinkedPropertyWrapper *>(this)->propName.empty()) {
							auto &a = *static_cast<Array *>(get_property_value(*prop));
							if(get_array_value_type(a) != Type::Element)
								return;
							auto &e = get_array_value<Element>(a, arrayIndex);
							remove_element_child(e, static_cast<const LinkedPropertyWrapper *>(this)->propName);
						}
						else
							throw LogicError {"Cannot assign nullopt value to array!"};
						return;
					}
					if constexpr(std::is_same_v<TBase, PProperty>) {
						if(linked && !static_cast<const LinkedPropertyWrapper *>(this)->propName.empty()) {
							auto &linked = *GetLinked();
							if(linked.prev && linked.prev->IsType(Type::Element)) {
								auto &el = linked.prev->GetValue<Element>();
								remove_element_child(el, linked.propName);
								return;
							}
						}
					}
					if(get_property_type(*prop) != Type::Element) {
						throw LogicError {"Cannot assign nullopt value to concrete UDM value!"};
						return;
					}
					if constexpr(type_to_enum_s<TBase>() != Type::Invalid) {
						if(get_property_value(*prop) == nullptr)
							throw LogicError {"Cannot assign property value: Property is invalid!"};
						auto &el = *static_cast<Element *>(get_property_value(*prop));
						auto &wpParent = get_element_parent_property(el);
						if(!wpParent)
							throw InvalidUsageError {"Attempted to change value of element property without a valid parent, this is not allowed!"};
						auto &parent = wpParent;
						switch(get_property_type(parent)) {
						case Type::Element:
							erase_element_child(*static_cast<Element *>(get_property_value(parent)), el);
							break;
						/*case Type::Array:
						if(arrayIndex == std::numeric_limits<uint32_t>::max())
							throw std::runtime_error{"Element has parent of type " +std::string{magic_enum::enum_name(parent->type)} +", but is not indexed!"};
						(*static_cast<Array*>(parent->value))[arrayIndex] = v;
						break;*/
						default:
							throw InvalidUsageError {
							  "Element has parent of type " + std::string {magic_enum::enum_name(get_property_type(parent))} + ", but only " + std::string {magic_enum::enum_name(Type::Element)} /* +" and " +std::string{magic_enum::enum_name(Type::Array)}*/ + " types are allowed!"};
						}
					}
					else
						throw LogicError {"Cannot assign custom type to non-struct property!"};
					return;
				}
				return operator=(*v);
			}
			else if constexpr(std::is_enum_v<std::remove_reference_t<TBase>>)
				return operator=(magic_enum::enum_name(v));
			else {
				if(is_array_type(get_property_type(*prop))) {
					if(arrayIndex == std::numeric_limits<uint32_t>::max())
						throw LogicError {"Cannot assign propety value to array: No index has been specified!"};
					if(linked && !static_cast<const LinkedPropertyWrapper *>(this)->propName.empty()) {
						auto &a = *static_cast<Array *>(get_property_value(*prop));
						if(get_array_value_type(a) != Type::Element)
							return;
						auto &e = get_array_value<Element>(a, arrayIndex);
						if constexpr(type_to_enum_s<TBase>() != Type::Invalid)
							set_element_child_value(e, static_cast<const LinkedPropertyWrapper *>(this)->propName, udm::create_property(v));
						else if constexpr(std::is_same_v<TBase, PProperty>)
							set_element_child_value(e, static_cast<const LinkedPropertyWrapper *>(this)->propName, v);
						else if constexpr(std::is_same_v<TBase, Property>)
							set_element_child_value(e, static_cast<const LinkedPropertyWrapper *>(this)->propName, udm::copy_property(v));
						else {
							auto *child = find_element_child(e, static_cast<const LinkedPropertyWrapper *>(this)->propName);
							if(!child || is_property_type(**child, Type::Struct) == false)
								throw LogicError {"Cannot assign custom type to non-struct property!"};
							auto &strct = get_property_value<Struct>(**child);
							set_struct_value(strct, &v, sizeof(T));
						}
					}
					else
						set_array_value(*static_cast<Array *>(get_property_value(*prop)), arrayIndex, v);
					return;
				}
				if constexpr(std::is_same_v<TBase, PProperty>) {
					if(linked && !static_cast<const LinkedPropertyWrapper *>(this)->propName.empty()) {
						auto &linked = *GetLinked();
						if(linked.prev && linked.prev->IsType(Type::Element)) {
							auto &el = linked.prev->GetValue<Element>();
							set_element_child_value(el, linked.propName, v);
							return;
						}
					}
				}
				if(get_property_type(*prop) != Type::Element) {
					set_property_value(*prop, v);
					return;
				}
				if constexpr(type_to_enum_s<TBase>() != Type::Invalid) {
					if(get_property_value(*prop) == nullptr)
						throw LogicError {"Cannot assign property value: Property is invalid!"};
					auto &el = *static_cast<Element *>(get_property_value(*prop));
					auto &wpParent = get_element_parent_property(el);
					if(!wpParent)
						throw InvalidUsageError {"Attempted to change value of element property without a valid parent, this is not allowed!"};
					auto &parent = wpParent;
					switch(get_property_type(parent)) {
					case Type::Element:
						{
							auto *valParent = static_cast<Element *>(get_property_value(parent));
							set_element_value(*valParent, el, v);
							break;
						}
					/*case Type::Array:
						if(arrayIndex == std::numeric_limits<uint32_t>::max())
							throw std::runtime_error{"Element has parent of type " +std::string{magic_enum::enum_name(parent->type)} +", but is not indexed!"};
						(*static_cast<Array*>(parent->value))[arrayIndex] = v;
						break;*/
					default:
						throw InvalidUsageError {
						  "Element has parent of type " + std::string {magic_enum::enum_name(get_property_type(parent))} + ", but only " + std::string {magic_enum::enum_name(Type::Element)} /* +" and " +std::string{magic_enum::enum_name(Type::Array)}*/ + " types are allowed!"};
					}
				}
				else
					throw LogicError {"Cannot assign custom type to non-struct property!"};
			}
		}

		template<typename T>
		LinkedPropertyWrapper PropertyWrapper::AddArray(const std::string_view &path, const StructDescription &strct, const T *data, uint32_t strctItems, ArrayType arrayType, bool pathToElements) const
		{
			auto prop = AddArray(path, strct, strctItems, arrayType, pathToElements);
			auto &a = prop.template GetValue<Array>();
			auto sz = get_array_value_size(a) * get_array_size(a);
			auto *ptr = get_array_values(a);
			memcpy(ptr, data, sz);
			return prop;
		}

		template<typename T>
		LinkedPropertyWrapper PropertyWrapper::AddArray(const std::string_view &path, const StructDescription &strct, const std::vector<T> &values, ArrayType arrayType, bool pathToElements) const
		{
			auto prop = AddArray(path, strct, values.size(), arrayType, pathToElements);
			auto &a = prop.template GetValue<Array>();
			auto sz = a.GetValueSize() * a.GetSize();
			auto szValues = pragma::util::size_of_container(values);
			if(szValues != sz)
				throw InvalidUsageError {"Size of values does not match expected size of defined struct!"};
			auto *ptr = a.GetValues();
			memcpy(ptr, values.data(), szValues);
			return prop;
		}

		template<typename T>
		LinkedPropertyWrapper PropertyWrapper::AddArray(const std::string_view &path, const std::vector<T> &values, ArrayType arrayType, bool pathToElements) const
		{
			return AddArray<T>(path, values.size(), values.data(), arrayType, pathToElements);
		}

		template<typename T>
		LinkedPropertyWrapper PropertyWrapper::AddArray(const std::string_view &path, uint32_t size, const T *data, ArrayType arrayType, bool pathToElements) const
		{
			constexpr auto valueType = type_to_enum<T>();
			auto prop = AddArray(path, size, valueType, arrayType, pathToElements);
			auto &a = prop.template GetValue<Array>();
			if constexpr(is_non_trivial_type(valueType) && valueType != Type::Struct) {
				for(auto i = decltype(size) {0u}; i < size; ++i)
					a[i] = data[i];
			}
			else
				memcpy(a.GetValues(), data, sizeof(T) * size);
			return prop;
		}

		template<class T>
		BlobResult PropertyWrapper::GetBlobData(T &v) const
		{
			uint64_t reqBufferSize = 0;
			auto result = GetBlobData(v.data(), v.size() * sizeof(v[0]), &reqBufferSize);
			if(result == BlobResult::InsufficientSize) {
				if(v.size() * sizeof(v[0]) != reqBufferSize) {
					if((reqBufferSize % sizeof(v[0])) > 0)
						return BlobResult::ValueTypeMismatch;
					v.resize(reqBufferSize / sizeof(v[0]));
					return GetBlobData<T>(v);
				}
				return result;
			}
			if(result != BlobResult::NotABlobType)
				return result;
			if(IsArrayItem(true)) {
				if(linked && !static_cast<const LinkedPropertyWrapper &>(*this).propName.empty()) {
					auto &a = get_property_value<Array>(*prop);
					auto *el = get_array_value_ptr<Element>(a, arrayIndex);
					if(!el)
						return BlobResult::InvalidProperty;
					auto *child = find_element_child(*el, static_cast<const LinkedPropertyWrapper &>(*this).propName);
					if(child)
						return get_property_blob_data<T>(**child, v);
					return BlobResult::InvalidProperty;
				}
				return BlobResult::NotABlobType;
			}
			return get_property_blob_data<T>(**this, v);
		}
		template<typename T>
		T &PropertyWrapper::GetValue() const
		{
			if(arrayIndex != std::numeric_limits<uint32_t>::max()) {
				auto *a = get_property_value_ptr<Array>(*prop);
				if(a) {
					if(linked && !static_cast<const LinkedPropertyWrapper &>(*this).propName.empty()) {
						auto &el = const_cast<Element &>(get_array_value<Element>(*a, arrayIndex));
						auto &propName = static_cast<const LinkedPropertyWrapper &>(*this).propName;
						auto *child = find_element_child(el, propName);
						if(!child)
							throw LogicError {"Attempted to retrieve value of property '" + propName + "' from array element at index " + std::to_string(arrayIndex) + ", but property does not exist!"};
						return get_property_value<T>(**child);
					}
					if(is_array_value_type(*a, type_to_enum<T>()) == false)
						throw LogicError {"Type mismatch, requested type is " + std::string {magic_enum::enum_name(type_to_enum<T>())} + ", but actual type is " + std::string {magic_enum::enum_name(get_array_value_type(*a))} + "!"};
					return static_cast<T *>(get_array_values(*a))[arrayIndex];
				}
			}
			return get_property_value<T>(**this);
		}

		template<typename T>
		T *PropertyWrapper::GetValuePtr() const
		{
			if(arrayIndex != std::numeric_limits<uint32_t>::max()) {
				auto *a = get_property_value_ptr<Array>(*prop);
				if(a) {
					if(linked && !static_cast<const LinkedPropertyWrapper &>(*this).propName.empty()) {
						auto &el = get_array_value<Element>(*a, arrayIndex);
						auto *child = find_element_child(el, static_cast<const LinkedPropertyWrapper &>(*this).propName);
						if(!child)
							return nullptr;
						return get_property_value_ptr<T>(**child);
					}
					if(is_array_value_type(*a, type_to_enum<T>()) == false)
						return nullptr;
					return &static_cast<T *>(get_array_values(*a))[arrayIndex];
				}
			}
			return prop ? get_property_value_ptr<T>(**this) : nullptr;
		}

		template<typename T>
		T PropertyWrapper::ToValue(const T &defaultValue, bool *optOutIsDefined) const
		{
			if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
			{
				if(optOutIsDefined)
					*optOutIsDefined = false;
				return defaultValue;
			}
			auto val = ToValue<T>();
			if(val.has_value()) {
				if(optOutIsDefined)
					*optOutIsDefined = true;
				return std::move(val.value());
			}
			if(optOutIsDefined)
				*optOutIsDefined = false;
			return defaultValue;
		}

		template<typename T>
		bool PropertyWrapper::operator==(const T &other) const
		{
			if constexpr(pragma::util::is_c_string<T>())
				return operator==(std::string {other});
			else {
				auto *val = GetValuePtr<T>();
				if(val)
					return *val == other;
				auto valConv = ToValue<T>();
				return valConv.has_value() ? *valConv == other : false;
			}
			return false;
		}

		template<typename T>
		ArrayIterator<T> PropertyWrapper::begin() const
		{
			if(!static_cast<bool>(*this))
				return ArrayIterator<T> {};
			auto *a = GetValuePtr<Array>();
			if(a == nullptr)
				return ArrayIterator<T> {};
			ArrayIterator<T> it;
			get_array_begin_iterator(*a, it);
			if(linked)
				it.GetProperty().prev = std::make_unique<LinkedPropertyWrapper>(*static_cast<LinkedPropertyWrapper *>(const_cast<PropertyWrapper *>(this)));
			return it;
		}
		template<typename T>
		ArrayIterator<T> PropertyWrapper::end() const
		{
			if(!static_cast<bool>(*this))
				return ArrayIterator<T> {};
			auto *a = GetValuePtr<Array>();
			if(a == nullptr)
				return ArrayIterator<T> {};
			ArrayIterator<T> it;
			get_array_end_iterator(*a, it);
			return it;
		}

		template<typename T>
		std::optional<T> PropertyWrapper::ToValue() const
		{
			if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
				return {};
			if(IsArrayItem(true)) {
				auto &a = get_property_value<Array>(*prop);
				if(linked && !static_cast<const LinkedPropertyWrapper &>(*this).propName.empty()) {
					auto &el = get_array_value<Element>(a, arrayIndex);
					auto *child = find_element_child(el, static_cast<const LinkedPropertyWrapper &>(*this).propName);
					if(!child)
						return {};
					return to_property_value<T>(**child);
				}
				auto vs = [&](auto tag) -> std::optional<T> {
					if constexpr(is_convertible<typename decltype(tag)::type, T>())
						return std::optional<T> {convert<typename decltype(tag)::type, T>(const_cast<PropertyWrapper *>(this)->GetValue<typename decltype(tag)::type>())};
					return {};
				};
				auto valueType = get_array_value_type(a);
				return visit(valueType, vs);
			}
			if(prop)
				return to_property_value<T>(**this);
			return std::optional<T> {};
		}
	}
}
