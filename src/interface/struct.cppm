// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include "sharedutils/magic_enum.hpp"
#include <string>
#include <vector>
#include <cstring>

export module pragma.udm:structure;

export import :basic_types;
export import :enums;
import :exception;
import :trivial_types;

export {
	namespace udm {
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

		template<typename T1, typename T2, typename... T>
		void StructDescription::DefineTypes(std::initializer_list<std::string>::iterator it)
		{
			DefineTypes<T1>(it);
			DefineTypes<T2, T...>(it + 1);
		}
		template<typename T>
		void StructDescription::DefineTypes(std::initializer_list<std::string>::iterator it)
		{
			types.push_back(type_to_enum<T>());
			names.push_back(*it);
		}

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
			void Assign(const void *inData, size_t inSize) {
				auto sz = description.GetDataSizeRequirement();
				if (inSize != sz)
					throw LogicError {"Attempted to assign data of size " + std::to_string(inSize) + " to struct of size " + std::to_string(sz) + "!"};
				if(data.size() != sz)
					throw ImplementationError {"Size of struct data does not match its types!"};
				memcpy(data.data(), inData, inSize);
			}
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
		template<class T>
		Struct &Struct::operator=(const T &other)
		{
			Assign(&other, sizeof(T));
			return *this;
		}
	}
}
