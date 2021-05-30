/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>

udm::ElementIteratorPair::ElementIteratorPair(std::unordered_map<std::string,PProperty>::iterator &it)
	: key{it->first},property{*it->second}
{}
udm::ElementIteratorPair::ElementIteratorPair()=default;
bool udm::ElementIteratorPair::operator==(const ElementIteratorPair &other) const
{
	return key == other.key && property == other.property;
}
bool udm::ElementIteratorPair::operator!=(const ElementIteratorPair &other) const {return !operator==(other);}

udm::ElementIteratorWrapper::ElementIteratorWrapper(LinkedPropertyWrapper &prop)
	: m_prop{prop}
{}

udm::ElementIterator udm::ElementIteratorWrapper::begin() {return m_prop.begin_el();}
udm::ElementIterator udm::ElementIteratorWrapper::end() {return m_prop.end_el();}

udm::ElementIterator::ElementIterator()
	: m_iterator{}
{}

udm::ElementIterator::ElementIterator(udm::Element &e)
	: ElementIterator{e,e.children.begin()}
{}

udm::ElementIterator::ElementIterator(udm::Element &e,std::unordered_map<std::string,PProperty>::iterator it)
	: m_iterator{it},m_pair{it}
{}

udm::ElementIterator::ElementIterator(const ElementIterator &other)
	: m_iterator{other.m_iterator},m_pair{other.m_pair}
{}

udm::ElementIterator &udm::ElementIterator::operator++()
{
	++m_iterator;
	m_pair = ElementIteratorPair{m_iterator};
	return *this;
}

udm::ElementIterator udm::ElementIterator::operator++(int)
{
	auto it = *this;
	it.operator++();
	return it;
}

typename udm::ElementIterator::reference udm::ElementIterator::operator*() {return m_pair;}

typename udm::ElementIterator::pointer udm::ElementIterator::operator->() {return &m_pair;}

bool udm::ElementIterator::operator==(const ElementIterator &other) const {return m_iterator == other.m_iterator;}

bool udm::ElementIterator::operator!=(const ElementIterator &other) const {return !operator==(other);}
