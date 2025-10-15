// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include "sharedutils/magic_enum.hpp"
#include <string>

export module pragma.udm:types.element;

export import :property_wrapper;
import :wrapper_funcs;

export {
	namespace udm {
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
			friend void erase_element_child(Element &e, Element &child);
			template<typename T>
				friend void set_element_value(Element &child, T &&v);

			template<typename T>
			void SetValue(Element &child, T &&v);
			void EraseValue(const Element &child);
		};
	}

	template<typename T>
	void udm::Element::SetValue(Element &child, T &&v)
	{
		auto it = std::find_if(children.begin(), children.end(), [&child](const std::pair<std::string, PProperty> &pair) {
			return get_property_type(*pair.second) == udm::Type::Element && get_property_value(*pair.second) == &child;
		});
		if(it == children.end())
			return;
		children[it->first] = create_property<T>(std::forward<T>(v));
	}
}
