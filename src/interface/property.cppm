// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"
#include <cassert>

export module pragma.udm:property;

import :array;
import :file;
import :types.element;
import :structure;
export import :trivial_types;
export import :types;
import :util;

export {
	namespace udm {
		struct DLLUDM Property {
			template<typename T>
			static void Construct(Property &prop, T &&value);
			template<typename T>
			static void Construct(Property &prop);
			static void Construct(Property &prop, Type type);

			template<typename T>
			static PProperty Create(T &&value);
			template<typename T>
			static PProperty Create();
			static PProperty Create() { return Create<Nil>(); }
			static PProperty Create(Type type);
			Property() = default;
			Property(const Property &other);
			Property(Property &&other);
			~Property();
			bool IsType(Type ptype) const { return ptype == type; }
			Property &operator=(const Property &other);
			Property &operator=(Property &&other) = delete;
			template<typename T>
			void operator=(T &&v);
			void Copy(const Property &other, bool deepCopy);
			PProperty Copy(bool deepCopy = false) const;

			Hash CalcHash() const;

			Type type = Type::Nil;
			DataValue value = nullptr;

			LinkedPropertyWrapper operator[](const std::string &key);
			LinkedPropertyWrapper operator[](const char *key);

			bool operator==(const Property &other) const;
			bool operator!=(const Property &other) const { return !operator==(other); }

			// operator LinkedPropertyWrapper();

			bool Compress();
			bool Decompress(const std::optional<Type> arrayValueType = {});

			BlobResult GetBlobData(void *outBuffer, size_t bufferSize, uint64_t *optOutRequiredSize = nullptr) const;
			BlobResult GetBlobData(void *outBuffer, size_t bufferSize, Type type, uint64_t *optOutRequiredSize = nullptr) const;
			Blob GetBlobData(Type &outType) const;
			template<class T>
			BlobResult GetBlobData(T &v) const;
			template<typename T>
			T &GetValue();
			template<typename T>
			const T &GetValue() const;
			template<typename T>
			T *GetValuePtr();
			void *GetValuePtr(Type &outType);
			template<typename T>
			T ToValue(const T &defaultValue) const;
			template<typename T>
			std::optional<T> ToValue() const;
			operator bool() const { return type != Type::Nil; }

			bool Read(IFile &f);
			bool Read(Type type, IFile &f);
			void Write(IFile &f) const;

			void ToAscii(AsciiSaveFlags flags, std::stringstream &ss, const std::string &propName, const std::string &prefix = "");

			static void ToAscii(AsciiSaveFlags flags, std::stringstream &ss, const std::string &propName, Type type, const DataValue value, const std::string &prefix = "");
			bool Read(IFile &f, Blob &outBlob);
			bool Read(IFile &f, BlobLz4 &outBlob);
			bool Read(IFile &f, Utf8String &outStr);
			bool Read(IFile &f, Element &outEl);
			bool Read(IFile &f, Array &outArray);
			bool Read(IFile &f, ArrayLz4 &outArray);
			static bool Read(IFile &f, String &outStr);
			bool Read(IFile &f, Reference &outRef);
			bool Read(IFile &f, Struct &strct);
			static void Write(IFile &f, const Blob &blob);
			static void Write(IFile &f, const BlobLz4 &blob);
			static void Write(IFile &f, const Utf8String &str);
			static void Write(IFile &f, const Element &el);
			static void Write(IFile &f, const Array &a);
			static void Write(IFile &f, const ArrayLz4 &a);
			static void Write(IFile &f, const String &str);
			static void Write(IFile &f, const Reference &ref);
			static void Write(IFile &f, const Struct &strct);

			static std::string ToAsciiValue(AsciiSaveFlags flags, const Nil &nil, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Blob &blob, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const BlobLz4 &blob, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Utf8String &utf8, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Element &el, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Array &a, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const ArrayLz4 &a, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const String &str, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Reference &ref, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Struct &strct, const std::string &prefix = "");
			static std::string StructToAsciiValue(AsciiSaveFlags flags, const StructDescription &strct, const void *data, const std::string &prefix = "");
			static void ArrayValuesToAscii(AsciiSaveFlags flags, std::stringstream &ss, const Array &a, const std::string &prefix = "");

			static std::string ToAsciiValue(AsciiSaveFlags flags, const Vector2 &v, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Vector2i &v, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Vector3 &v, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Vector3i &v, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Vector4 &v, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Vector4i &v, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Quaternion &q, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const EulerAngles &a, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Srgba &srgb, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const HdrColor &col, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Transform &t, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const ScaledTransform &t, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Mat4 &m, const std::string &prefix = "");
			static std::string ToAsciiValue(AsciiSaveFlags flags, const Mat3x4 &m, const std::string &prefix = "");

			static constexpr uint8_t EXTENDED_STRING_IDENTIFIER = std::numeric_limits<uint8_t>::max();
			static BlobResult GetBlobData(const Blob &blob, void *outBuffer, size_t bufferSize);
			static BlobResult GetBlobData(const BlobLz4 &blob, void *outBuffer, size_t bufferSize);
			static Blob GetBlobData(const BlobLz4 &blob);
			static uint32_t GetStringPrefixSizeRequirement(const String &str);
			static uint32_t GetStringSizeRequirement(const String &str);
		  private:
			friend PropertyWrapper;
			bool ReadStructHeader(IFile &f, StructDescription &strct);
			static void WriteStructHeader(IFile &f, const StructDescription &strct);
			template<bool ENABLE_EXCEPTIONS, typename T>
			bool Assign(T &&v);

			template<typename T>
			static uint64_t WriteBlockSize(IFile &f);
			template<typename T>
			static void WriteBlockSize(IFile &f, uint64_t offset);

			template<typename T>
			static void NumericTypeToString(T value, std::stringstream &ss);
			template<typename T>
			static std::string NumericTypeToString(T value);
			static void SetAppropriatePrecision(std::stringstream &ss, Type type);
			static void RemoveTrailingZeroes(std::string &str);
			void Initialize();
			void Clear();
			template<typename T>
			T &GetValue(Type type);
		};

		template<bool ENABLE_EXCEPTIONS, typename T>
		bool Property::Assign(T &&v)
		{
			using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr(pragma::util::is_specialization<TBase, std::vector>::value) {
				using TValueType = typename TBase::value_type;
				if(!is_array_type(type)) {
					if constexpr(ENABLE_EXCEPTIONS)
						throw InvalidUsageError {"Attempted to assign vector to non-array property (of type " + std::string {magic_enum::enum_name(type)} + "), this is not allowed!"};
					else
						return false;
				}
				auto valueType = type_to_enum<TValueType>();
				auto size = v.size();
				auto &a = *static_cast<Array *>(value);
				a.Clear();
				a.SetValueType(valueType);
				a.Resize(size);

				if(size_of_base_type(valueType) != sizeof(typename TBase::value_type)) {
					if constexpr(ENABLE_EXCEPTIONS)
						throw InvalidUsageError {"Type size mismatch!"};
					else
						return false;
				}
				auto vs = [this, &a, &v](auto tag) {
					using TTag = typename decltype(tag)::type;
					memcpy(a.GetValues(), v.data(), v.size() * sizeof(v[0]));
				};
				if(is_ng_type(valueType))
					visit_ng(valueType, vs);
				else if(is_non_trivial_type(valueType)) {
					// Elements have to be copied explicitly
					for(auto i = decltype(size) {0u}; i < size; ++i)
						a[i] = v[i];
				}
				else
					return false;
				return true;
			}
			else if constexpr(pragma::util::is_specialization<TBase, std::unordered_map>::value || pragma::util::is_specialization<TBase, std::map>::value) {
				if(type != Type::Element) {
					if constexpr(ENABLE_EXCEPTIONS)
						throw InvalidUsageError {"Attempted to assign map to non-element property (of type " + std::string {magic_enum::enum_name(type)} + "), this is not allowed!"};
					else
						return false;
				}
				for(auto &pair : v)
					(*this)[pair.first] = pair.second;
				return true;
			}
			auto vType = type_to_enum_s<TBase>();
			if(vType == Type::Invalid) {
				if constexpr(ENABLE_EXCEPTIONS)
					throw LogicError {"Attempted to assign value of type '" + std::string {typeid(T).name()} + "', which is not a recognized type!"};
				else
					return false;
			}
			auto vs = [this, &v](auto tag) {
				using TTag = typename decltype(tag)::type;
				if constexpr(is_convertible<TBase, TTag>()) {
					*static_cast<TTag *>(value) = convert<TBase, TTag>(v);
					return true;
				}
				else
					return false;
			};
			return visit(vType, vs);
		}

		template<typename T>
		void Property::operator=(T &&v)
		{
			Assign<true, T>(std::forward<T>(v));
		}

		template<typename T>
		PProperty Property::Create(T &&value)
		{
			auto prop = Create<T>();
			*prop = value;
			return prop;
		}

		template<typename T>
		void Property::Construct(Property &prop, T &&value)
		{
			prop = value;
		}

		template<typename T>
		PProperty Property::Create()
		{
			using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
			return Create(type_to_enum<TBase>());
		}

		template<typename T>
		void Property::Construct(Property &prop)
		{
			using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
			Construct(prop, type_to_enum<TBase>());
		}

		template<typename T>
		uint64_t Property::WriteBlockSize(IFile &f)
		{
			auto offsetToSize = f.Tell();
			f.Write<T>(0);
			return offsetToSize;
		}
		template<typename T>
		void Property::WriteBlockSize(IFile &f, uint64_t offset)
		{
			auto startOffset = offset + sizeof(T);
			auto curOffset = f.Tell();
			f.Seek(offset);
			f.Write<T>(curOffset - startOffset);
			f.Seek(curOffset);
		}

		template<typename T>
		void Property::NumericTypeToString(T value, std::stringstream &ss)
		{
			using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr(std::is_same_v<TBase, Half>) {
				NumericTypeToString<float>(value, ss);
				return;
			}
			if constexpr(!std::is_floating_point_v<T>) {
				if constexpr(std::is_same_v<T, Int8> || std::is_same_v<T, UInt8>)
					ss << +value;
				else
					ss << value;
				return;
			}
			// SetAppropriatePrecision(ss,type_to_enum<T>());
			ss << NumericTypeToString(value);
		}
		template<typename T>
		std::string Property::NumericTypeToString(T value)
		{
			using TBase = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr(std::is_same_v<TBase, Half>)
				return NumericTypeToString<float>(value);
			if constexpr(!std::is_floating_point_v<T>) {
				if constexpr(std::is_same_v<T, Int8> || std::is_same_v<T, UInt8>)
					return std::to_string(+value);
				return std::to_string(value);
			}
			// TODO: This is not very efficient...
			// (We need a temporary stringstream because we want to
			// remove trailing zeroes)
			std::stringstream tmp;
			SetAppropriatePrecision(tmp, type_to_enum<T>());
			if constexpr(std::is_same_v<T, Int8> || std::is_same_v<T, UInt8>)
				tmp << +value;
			else
				tmp << value;
			auto str = tmp.str();
			RemoveTrailingZeroes(str);
			return str;
		}

		template<typename T>
		T &Property::GetValue()
		{
			return GetValue<T>(type_to_enum<T>());
		}

		template<typename T>
		const T &Property::GetValue() const
		{
			return const_cast<Property *>(this)->GetValue<T>();
		}
		template<typename T>
		T Property::ToValue(const T &defaultValue) const
		{
			if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
				return defaultValue;
			auto val = ToValue<T>();
			return val.has_value() ? *val : defaultValue;
		}
		template<typename T>
		std::optional<T> Property::ToValue() const
		{
			if(!this) // This can happen in chained expressions. TODO: This is technically undefined behavior and should be implemented differently!
				return {};
			if constexpr(pragma::util::is_specialization<T, std::vector>::value) {
				T v {};
				auto res = GetBlobData(v);
				return (res == BlobResult::Success) ? v : std::optional<T> {};
			}
			else if constexpr(pragma::util::is_specialization<T, std::unordered_map>::value || pragma::util::is_specialization<T, std::map>::value) {
				if(type != Type::Element)
					return {};
				using TValue = decltype(T::value_type::second);
				auto &e = GetValue<Element>();
				T result {};
				for(auto &pair : e.children) {
					auto val = pair.second->ToValue<TValue>();
					if(val.has_value() == false)
						continue;
					result[pair.first] = std::move(val.value());
				}
				return result;
			}
			auto vs = [&](auto tag) -> std::optional<T> {
				if constexpr(is_convertible<typename decltype(tag)::type, T>())
					return convert<typename decltype(tag)::type, T>(const_cast<Property *>(this)->GetValue<typename decltype(tag)::type>());
				return {};
			};
			return visit(type, vs);
			static_assert(pragma::math::to_integral(Type::Count) == 36, "Update this list when new types are added!");
			return {};
		}

		template<typename T>
		T &Property::GetValue(Type type)
		{
			assert(value && this->type == type);
			if(this->type != type && !(this->type == Type::ArrayLz4 && type == Type::Array))
				throw LogicError {"Type mismatch, requested type is " + std::string {magic_enum::enum_name(type)} + ", but actual type is " + std::string {magic_enum::enum_name(this->type)} + "!"};
			return *GetValuePtr<T>();
		}

		template<typename T>
		T *Property::GetValuePtr()
		{
			// TODO: this should never be null, but there are certain cases where it seems to happen
			if(!this)
				return nullptr;
			if constexpr(std::is_same_v<T, Array>)
				return is_array_type(this->type) ? reinterpret_cast<T *>(value) : nullptr;
			return (this->type == type_to_enum<T>()) ? reinterpret_cast<T *>(value) : nullptr;
		}

		template<class T>
		BlobResult Property::GetBlobData(T &v) const
		{
			if(!*this)
				return BlobResult::InvalidProperty;
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
			if constexpr(is_trivial_type(type_to_enum_s<typename T::value_type>())) {
				if(is_array_type(this->type)) {
					auto &a = GetValue<Array>();
					if(a.GetValueType() == type_to_enum<typename T::value_type>()) {
						v.resize(a.GetSize());
						memcpy(v.data(), a.GetValues(), v.size() * sizeof(v[0]));
						return BlobResult::Success;
					}
				}
			}
			return BlobResult::NotABlobType;
		}
	}
}
