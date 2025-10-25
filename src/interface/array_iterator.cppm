// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include "sharedutils/magic_enum.hpp"
#include <string>

export module pragma.udm:array_iterator;

export import :property_wrapper;
export import :types;
import :wrapper_funcs;

export {
	namespace udm {
		template<typename T>
		class ArrayIterator {
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = T &;
			using difference_type = std::ptrdiff_t;
			using pointer = T *;
			using reference = T &;

			ArrayIterator();
			explicit ArrayIterator(udm::Array &a);
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

		template<typename T>
		ArrayIterator<T>::ArrayIterator() : m_curProperty {}
		{
		}

		template<typename T>
		ArrayIterator<T>::ArrayIterator(Array &a, uint32_t idx) : m_curProperty {a, idx}
		{
		}

		template<typename T>
		ArrayIterator<T>::ArrayIterator(Array &a) : ArrayIterator {a, 0u}
		{
		}

		template<typename T>
		ArrayIterator<T>::ArrayIterator(const ArrayIterator &other) : m_curProperty {other.m_curProperty}
		{
		}

		template<typename T>
		ArrayIterator<T> &ArrayIterator<T>::operator++()
		{
			++m_curProperty.arrayIndex;
			return *this;
		}

		template<typename T>
		ArrayIterator<T> ArrayIterator<T>::operator++(int)
		{
			auto it = *this;
			it.operator++();
			return it;
		}

		template<typename T>
		ArrayIterator<T> ArrayIterator<T>::operator+(uint32_t n)
		{
			auto it = *this;
			for(auto i = decltype(n) {0u}; i < n; ++i)
				it.operator++();
			return it;
		}

		template<typename T>
		typename ArrayIterator<T>::reference ArrayIterator<T>::operator*()
		{
			if constexpr(std::is_same_v<T, LinkedPropertyWrapper>)
				return m_curProperty;
			else
				return get_property_value<T>(m_curProperty);
		}

		template<typename T>
		typename ArrayIterator<T>::pointer ArrayIterator<T>::operator->()
		{
			if constexpr(std::is_same_v<T, LinkedPropertyWrapper>)
				return &m_curProperty;
			else
				return get_property_value_ptr<T>(m_curProperty);
		}

		template<typename T>
		bool ArrayIterator<T>::operator==(const ArrayIterator &other) const
		{
			auto res = (m_curProperty == other.m_curProperty);
			// UDM_ASSERT_COMPARISON(res);
			return res;
		}

		template<typename T>
		bool ArrayIterator<T>::operator!=(const ArrayIterator &other) const
		{
			return !operator==(other);
		}
	}
}
