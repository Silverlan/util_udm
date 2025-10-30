// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <lz4.h>

module pragma.udm;

import :core;

udm::ElementIteratorPair::ElementIteratorPair(std::unordered_map<std::string, PProperty>::iterator &it) : key {it->first}, property {*it->second} {}
udm::ElementIteratorPair::ElementIteratorPair() = default;
bool udm::ElementIteratorPair::operator==(const ElementIteratorPair &other) const { return key == other.key && property == other.property; }
bool udm::ElementIteratorPair::operator!=(const ElementIteratorPair &other) const { return !operator==(other); }

udm::ElementIteratorWrapper::ElementIteratorWrapper(LinkedPropertyWrapper &prop) : m_prop {prop} {}

udm::ElementIterator udm::ElementIteratorWrapper::begin() { return m_prop.begin_el(); }
udm::ElementIterator udm::ElementIteratorWrapper::end() { return m_prop.end_el(); }

udm::ElementIterator::ElementIterator() : m_iterator {} {}

udm::ElementIterator::ElementIterator(udm::Element &e) : ElementIterator {e, e.children, e.children.begin()} {}

udm::ElementIterator::ElementIterator(udm::Element &e, util::StringMap<PProperty> &c, util::StringMap<PProperty>::iterator it) : m_iterator {it}, m_pair {}, m_propertyMap {&c}
{
	if(it != c.end())
		m_pair = {it};
}

udm::ElementIterator::ElementIterator(const ElementIterator &other) : m_iterator {other.m_iterator}, m_pair {other.m_pair}, m_propertyMap {other.m_propertyMap} {}

udm::ElementIterator &udm::ElementIterator::operator++()
{
	++m_iterator;
	m_pair = (m_propertyMap && m_iterator != m_propertyMap->end()) ? ElementIteratorPair {m_iterator} : ElementIteratorPair {};
	return *this;
}

udm::ElementIterator udm::ElementIterator::operator++(int)
{
	auto it = *this;
	it.operator++();
	return it;
}

typename udm::ElementIterator::reference udm::ElementIterator::operator*() { return m_pair; }

typename udm::ElementIterator::pointer udm::ElementIterator::operator->() { return &m_pair; }

bool udm::ElementIterator::operator==(const ElementIterator &other) const { return m_iterator == other.m_iterator; }

bool udm::ElementIterator::operator!=(const ElementIterator &other) const { return !operator==(other); }
