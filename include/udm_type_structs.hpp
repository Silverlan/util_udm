/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_TYPE_STRUCTS_HPP__
#define __UDM_TYPE_STRUCTS_HPP__

#include "udm_definitions.hpp"
#include "udm_exception.hpp"
#include "udm_enums.hpp"
#include "udm_basic_types.hpp"
#include <sharedutils/util_string_hash.hpp>
#include <cinttypes>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace udm {
#pragma pack(push, 1)
	struct DLLUDM Half {
		Half() = default;
		Half(uint16_t value) : value {value} {}
		Half(const Half &other) = default;
		Half(float value);
		operator float() const;
		Half &operator=(float value);
		Half &operator=(uint16_t value);
		Half &operator=(const Half &other) = default;
		uint16_t value;
	};
#pragma pack(pop)
	static_assert(sizeof(Half) == sizeof(uint16_t));

	struct DLLUDM Blob {
		Blob() = default;
		Blob(const Blob &) = default;
		Blob(Blob &&) = default;
		Blob(std::vector<uint8_t> &&data) : data {data} {}
		std::vector<uint8_t> data;

		Blob &operator=(Blob &&other);
		Blob &operator=(const Blob &other);

		bool operator==(const Blob &other) const
		{
			auto res = (data == other.data);
			UDM_ASSERT_COMPARISON(res);
			return res;
		}
		bool operator!=(const Blob &other) const { return !operator==(other); }
	};

	struct DLLUDM BlobLz4 {
		BlobLz4() = default;
		BlobLz4(const BlobLz4 &) = default;
		BlobLz4(BlobLz4 &&) = default;
		BlobLz4(std::vector<uint8_t> &&compressedData, size_t uncompressedSize) : compressedData {compressedData}, uncompressedSize {uncompressedSize} {}
		size_t uncompressedSize = 0;
		std::vector<uint8_t> compressedData;

		BlobLz4 &operator=(BlobLz4 &&other);
		BlobLz4 &operator=(const BlobLz4 &other);

		bool operator==(const BlobLz4 &other) const
		{
			auto res = (uncompressedSize == other.uncompressedSize && compressedData == other.compressedData);
			UDM_ASSERT_COMPARISON(res);
			return res;
		}
		bool operator!=(const BlobLz4 &other) const { return !operator==(other); }
	};

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

	template<typename T>
	class ArrayIterator;
	struct DLLUDM PropertyWrapper {
		PropertyWrapper() = default;
		PropertyWrapper(Property &o);
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
			if constexpr(util::is_specialization<TBase, std::optional>::value) {
				typename TBase::value_type v;
				if(!(*this)(v)) {
					valOut = {};
					return false;
				}
				valOut = v;
				return true;
			}
			else if constexpr(util::is_specialization<TBase, std::vector>::value || util::is_specialization<TBase, std::map>::value || util::is_specialization<TBase, std::unordered_map>::value) {
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
		Property *operator*() const;
		Property *operator->() const { return operator*(); }
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

	template<typename T>
	class ArrayIterator {
	  public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = T &;
		using difference_type = std::ptrdiff_t;
		using pointer = T *;
		using reference = T &;

		ArrayIterator();
		ArrayIterator(udm::Array &a);
		ArrayIterator(udm::Array &a, uint32_t idx);
		ArrayIterator(const ArrayIterator &other);
		ArrayIterator &operator++();
		ArrayIterator operator++(int);
		ArrayIterator operator+(uint32_t n);
		reference operator*();
		pointer operator->();
		bool operator==(const ArrayIterator &other) const;
		bool operator!=(const ArrayIterator &other) const;

		udm::LinkedPropertyWrapper &GetProperty() { return m_curProperty; }
	  private:
		udm::LinkedPropertyWrapper m_curProperty;
	};

	struct DLLUDM Reference {
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

	struct DLLUDM StructDescription {
		using SizeType = uint16_t;
		using MemberCountType = uint8_t;
		std::string GetTemplateArgumentList() const;
		SizeType GetDataSizeRequirement() const;
		MemberCountType GetMemberCount() const;

		// TODO: Use these once C++20 is available
		// bool operator==(const Struct&) const=default;
		// bool operator!=(const Struct&) const=default;
		bool operator==(const StructDescription &other) const;
		bool operator!=(const StructDescription &other) const { return !operator==(other); }

		template<typename T1, typename T2, typename... T>
		static StructDescription Define(std::initializer_list<std::string> names)
		{
			StructDescription strct {};
			strct.DefineTypes<T1, T2, T...>(names);
			return strct;
		}

		template<typename T1, typename T2, typename... T>
		void DefineTypes(std::initializer_list<std::string> names)
		{
			Clear();
			constexpr auto n = sizeof...(T) + 2;
			if(names.size() != n)
				throw InvalidUsageError {"Number of member names has to match number of member types!"};
			types.reserve(n);
			this->names.reserve(n);
			DefineTypes<T1, T2, T...>(names.begin());
		}

		void Clear()
		{
			types.clear();
			names.clear();
		}
		std::vector<Type> types;
		std::vector<String> names;
	  private:
		template<typename T1, typename T2, typename... T>
		void DefineTypes(std::initializer_list<std::string>::iterator it);
		template<typename T>
		void DefineTypes(std::initializer_list<std::string>::iterator it);
	};

	struct DLLUDM Struct {
		static constexpr auto MAX_SIZE = std::numeric_limits<StructDescription::SizeType>::max();
		static constexpr auto MAX_MEMBER_COUNT = std::numeric_limits<StructDescription::MemberCountType>::max();
		Struct() = default;
		Struct(const Struct &) = default;
		Struct(Struct &&) = default;
		Struct(const StructDescription &desc);
		Struct(StructDescription &&desc);
		Struct &operator=(const Struct &) = default;
		Struct &operator=(Struct &&) = default;
		template<class T>
		Struct &operator=(const T &other);
		// TODO: Use these once C++20 is available
		// bool operator==(const Struct&) const=default;
		// bool operator!=(const Struct&) const=default;
		bool operator==(const Struct &other) const;
		bool operator!=(const Struct &other) const { return !operator==(other); }

		StructDescription &operator*() { return description; }
		const StructDescription &operator*() const { return const_cast<Struct *>(this)->operator*(); }
		StructDescription *operator->() { return &description; }
		const StructDescription *operator->() const { return const_cast<Struct *>(this)->operator->(); }

		void Clear();
		void UpdateData();
		void SetDescription(const StructDescription &desc);
		void SetDescription(StructDescription &&desc);
		StructDescription description;
		std::vector<uint8_t> data;
	};

	struct DLLUDM ElementIteratorPair {
		ElementIteratorPair(util::StringMap<PProperty>::iterator &it);
		ElementIteratorPair();
		bool operator==(const ElementIteratorPair &other) const;
		bool operator!=(const ElementIteratorPair &other) const;
		std::string_view key;
		LinkedPropertyWrapper property;
	};

	class DLLUDM ElementIterator {
	  public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = ElementIteratorPair &;
		using difference_type = std::ptrdiff_t;
		using pointer = ElementIteratorPair *;
		using reference = ElementIteratorPair &;

		ElementIterator();
		ElementIterator(udm::Element &e);
		ElementIterator(udm::Element &e, util::StringMap<PProperty> &c, util::StringMap<PProperty>::iterator it);
		ElementIterator(const ElementIterator &other);
		ElementIterator &operator++();
		ElementIterator operator++(int);
		reference operator*();
		pointer operator->();
		bool operator==(const ElementIterator &other) const;
		bool operator!=(const ElementIterator &other) const;
	  private:
		util::StringMap<PProperty> *m_propertyMap = nullptr;
		util::StringMap<PProperty>::iterator m_iterator {};
		ElementIteratorPair m_pair;
	};

	struct DLLUDM Array {
		virtual ~Array();
		PropertyWrapper fromProperty {};

		bool operator==(const Array &other) const;
		bool operator!=(const Array &other) const { return !operator==(other); }

		void Merge(const Array &other, MergeFlags mergeFlags = MergeFlags::OverwriteExisting);

		virtual Array &operator=(Array &&other);
		virtual Array &operator=(const Array &other);

		virtual void SetValueType(Type valueType);
		virtual ArrayType GetArrayType() const { return ArrayType::Raw; }
		bool IsValueType(Type pvalueType) const { return pvalueType == m_valueType; }
		Type GetValueType() const { return m_valueType; }
		uint32_t GetSize() const { return m_size; }
		uint32_t GetValueSize() const;
		virtual void *GetValues() { return GetValuePtr(); }
		const void *GetValues() const { return const_cast<Array *>(this)->GetValues(); }
		void Resize(uint32_t newSize);
		void AddValueRange(uint32_t startIndex, uint32_t count);
		void RemoveValueRange(uint32_t startIndex, uint32_t count);
		void *GetValuePtr(uint32_t idx);
		template<typename T>
		T *GetValuePtr(uint32_t idx);
		template<typename T>
		const T *GetValuePtr(uint32_t idx) const
		{
			return const_cast<Array *>(this)->GetValuePtr<T>(idx);
		}
		PropertyWrapper operator[](uint32_t idx);
		const PropertyWrapper operator[](uint32_t idx) const { return const_cast<Array *>(this)->operator[](idx); }
		template<typename T>
		T &GetValue(uint32_t idx);
		template<typename T>
		const T &GetValue(uint32_t idx) const
		{
			return const_cast<Array *>(this)->GetValue<T>(idx);
		}
		template<typename T>
		void SetValue(uint32_t idx, T &&value);
		template<typename T>
		void InsertValue(uint32_t idx, T &&value);
		void RemoveValue(uint32_t idx);
		// The caller is responsible to ensure that the type of value matches the value type of the array!
		void SetValue(uint32_t idx, const void *value);
		void InsertValue(uint32_t idx, void *value);

		bool IsEmpty() const { return m_size == 0; }
		template<typename T>
		T *GetFront()
		{
			return !IsEmpty() ? GetValuePtr<T>(0u) : nullptr;
		}
		template<typename T>
		const T *GetFront() const
		{
			return const_cast<Array *>(this)->GetFront<T>();
		}
		template<typename T>
		T *GetBack()
		{
			return !IsEmpty() ? GetValuePtr<T>(m_size - 1) : nullptr;
		}
		template<typename T>
		const T *GetBack() const
		{
			return const_cast<Array *>(this)->GetBack<T>();
		}

		template<typename T>
		ArrayIterator<T> begin()
		{
			return ArrayIterator<T> {*this};
		}
		template<typename T>
		ArrayIterator<T> end()
		{
			return ArrayIterator<T> {*this, GetSize()};
		}
		ArrayIterator<LinkedPropertyWrapper> begin() { return begin<LinkedPropertyWrapper>(); }
		ArrayIterator<LinkedPropertyWrapper> end() { return end<LinkedPropertyWrapper>(); }

		uint64_t GetByteSize() const;
		const StructDescription *GetStructuredDataInfo() const { return const_cast<Array *>(this)->GetStructuredDataInfo(); }
		virtual StructDescription *GetStructuredDataInfo();

		static constexpr bool IsValueTypeSupported(Type type) { return true; }

		using Range = std::tuple<uint32_t, uint32_t, uint32_t>;
		void Resize(uint32_t newSize, Range r0, Range r1, bool defaultInitializeNewValues);
	  protected:
		friend Property;
		friend PropertyWrapper;
		virtual void Clear();

		void *GetValuePtr();
		const void *GetValuePtr() const { return const_cast<Array *>(this)->GetValuePtr(); }
		void *GetHeaderPtr();
		const void *GetHeaderPtr() const { return const_cast<Array *>(this)->GetHeaderPtr(); }
		uint64_t GetHeaderSize() const;
		void ReleaseValues();
		uint8_t *AllocateData(uint64_t size) const;

		void *m_values = nullptr;
		uint32_t m_size = 0;
		Type m_valueType = Type::Nil;
	};

	struct DLLUDM ArrayLz4 : public Array {
		enum class Flags : uint8_t {
			None = 0u,
			Compressed = 1u,
			PersistentUncompressedData = Compressed << 1u,
		};

		ArrayLz4() = default;
		virtual ArrayLz4 &operator=(Array &&other) override;
		virtual ArrayLz4 &operator=(const Array &other) override;
		ArrayLz4 &operator=(ArrayLz4 &&other);
		ArrayLz4 &operator=(const ArrayLz4 &other);
		const BlobLz4 &GetCompressedBlob() const { return const_cast<ArrayLz4 *>(this)->GetCompressedBlob(); }
		BlobLz4 &GetCompressedBlob();
		virtual void *GetValues() override;
		virtual void SetValueType(Type valueType) override;
		virtual ArrayType GetArrayType() const { return ArrayType::Compressed; }
		void ClearUncompressedMemory();
		void SetUncompressedMemoryPersistent(bool persistent);
		using Array::GetStructuredDataInfo;

		static constexpr bool IsValueTypeSupported(Type type);
	  private:
		friend Property;
		friend PropertyWrapper;
		friend AsciiReader;
		virtual StructDescription *GetStructuredDataInfo() override;
		void InitializeSize(uint32_t size);
		void Decompress();
		void Compress();
		virtual void Clear() override;
		std::unique_ptr<StructDescription> m_structuredDataInfo = nullptr;
		Flags m_flags = Flags::None;
		BlobLz4 m_compressedBlob {};
	};

	struct DLLUDM ElementIteratorWrapper {
		ElementIteratorWrapper(LinkedPropertyWrapper &prop);
		ElementIterator begin();
		ElementIterator end();
	  private:
		LinkedPropertyWrapper m_prop;
	};

	struct DLLUDM Element {
		void AddChild(std::string &&key, const PProperty &o);
		void AddChild(const std::string &key, const PProperty &o);
		void Copy(const Element &other);
		util::StringMap<PProperty> children;
		PropertyWrapper fromProperty {};
		PropertyWrapper parentProperty {};

		LinkedPropertyWrapper operator[](const std::string &key) { return fromProperty[key]; }
		LinkedPropertyWrapper operator[](const char *key) { return operator[](std::string {key}); }

		LinkedPropertyWrapper Add(const std::string_view &path, Type type = Type::Element, bool pathToElements = false);
		LinkedPropertyWrapper AddArray(const std::string_view &path, std::optional<uint32_t> size = {}, Type type = Type::Element, ArrayType arrayType = ArrayType::Raw, bool pathToElements = false);
		void ToAscii(AsciiSaveFlags flags, std::stringstream &ss, const std::optional<std::string> &prefix = {}) const;

		void Merge(const Element &other, MergeFlags mergeFlags = MergeFlags::OverwriteExisting);

		bool operator==(const Element &other) const;
		bool operator!=(const Element &other) const { return !operator==(other); }
		Element &operator=(Element &&other);
		Element &operator=(const Element &other);

		explicit operator PropertyWrapper &() { return fromProperty; }

		ElementIterator begin();
		ElementIterator end();
	  private:
		friend PropertyWrapper;
		template<typename T>
		void SetValue(Element &child, T &&v);
		template<typename T>
		void EraseValue(const Element &child);
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(udm::ArrayLz4::Flags)

#endif
