// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;


#include "udm_definitions.hpp"
#include <cassert>

module pragma.udm;

import :core;

udm::LinkedPropertyWrapper udm::Element::AddArray(const std::string_view &path, std::optional<uint32_t> size, Type type, ArrayType arrayType, bool pathToElements)
{
	auto prop = Add(path, (arrayType == ArrayType::Compressed) ? Type::ArrayLz4 : Type::Array, pathToElements);
	if(!prop)
		return prop;
	auto &a = *static_cast<Array *>(prop->value);
	a.SetValueType(type);
	if(size.has_value())
		a.Resize(*size);
	return prop;
}

udm::LinkedPropertyWrapper udm::Element::Add(const std::string_view &path, Type type, bool pathToElements)
{
	auto end = pathToElements ? path.find(PATH_SEPARATOR) : std::string::npos;
	auto name = path.substr(0, end);
	if(name.empty())
		return fromProperty;
	std::string strName {name};
	auto isLast = (end == std::string::npos);
	auto it = children.find(strName);
	if(isLast && it != children.end() && it->second->type != type) {
		children.erase(it);
		it = children.end();
	}
	if(it == children.end()) {
		if(isLast)
			AddChild(strName, Property::Create(type));
		else
			AddChild(strName, Property::Create<Element>());
		it = children.find(strName);
		assert(it != children.end());
	}
	if(isLast)
		return *it->second;
	return static_cast<Element *>(it->second->value)->Add(path.substr(end + 1), type);
}

bool udm::Element::operator==(const Element &other) const
{
	for(auto &pair : children) {
		auto it = other.children.find(pair.first);
		auto res = (it != other.children.end());
		UDM_ASSERT_COMPARISON(res);
		if(!res)
			return false;
		res = (*pair.second == *it->second);
		UDM_ASSERT_COMPARISON(res);
		if(!res)
			return false;
	}
	return true;
}

udm::Element &udm::Element::operator=(Element &&other)
{
	if(this == &other)
		return *this;
	children = std::move(other.children);
	fromProperty = other.fromProperty;
	parentProperty = other.parentProperty;
	return *this;
}
udm::Element &udm::Element::operator=(const Element &other)
{
	if(this == &other)
		return *this;
	children = other.children;
	fromProperty = other.fromProperty;
	parentProperty = other.parentProperty;
	return *this;
}
void udm::Element::Copy(const Element &other)
{
	for(auto &pair : other.children)
		AddChild(pair.first, pair.second->Copy(true));
}

void udm::Element::ToAscii(AsciiSaveFlags flags, std::stringstream &ss, const std::optional<std::string> &prefix) const
{
	auto childPrefix = prefix.has_value() ? (*prefix + '\t') : "";
	auto first = true;

	// We want to sort the children by name to have some consistency
	std::vector<std::string_view> names;
	names.reserve(children.size());
	for(auto &[name, child] : children)
		names.push_back(name);
	std::sort(names.begin(), names.end());

	for(auto &name : names) {
		auto it = children.find(name);
		assert(it != children.end());
		if(it == children.end())
			throw ImplementationError {"Failed to find key with name '" + std::string {name} + "'!"};
		if(first)
			first = false;
		else
			ss << "\n";
		it->second->ToAscii(flags, ss, it->first, childPrefix);
	}
}

void udm::Element::Merge(const Element &other, MergeFlags mergeFlags)
{
	for(auto &pair : other.children) {
		auto &prop = *pair.second;
		if(!prop.IsType(Type::Element) && !is_array_type(prop.type)) {
			AddChild(pair.first, umath::is_flag_set(mergeFlags, MergeFlags::DeepCopy) ? pair.second->Copy(true) : pair.second);
			continue;
		}
		auto it = children.find(pair.first);
		if(it == children.end() || (prop.type != it->second->type && (!is_array_type(prop.type) || !is_array_type(it->second->type)))) {
			if(it != children.end() && umath::is_flag_set(mergeFlags, MergeFlags::OverwriteExisting) == false)
				continue;
			AddChild(pair.first, umath::is_flag_set(mergeFlags, MergeFlags::DeepCopy) ? pair.second->Copy(true) : pair.second);
			continue;
		}
		if(prop.IsType(Type::Element)) {
			it->second->GetValue<Element>().Merge(prop.GetValue<Element>(), mergeFlags);
			continue;
		}
		// Array property
		it->second->GetValue<Array>().Merge(prop.GetValue<Array>(), mergeFlags);
	}
}

udm::ElementIterator udm::Element::begin() { return ElementIterator {*this, children, children.begin()}; }
udm::ElementIterator udm::Element::end() { return ElementIterator {*this, children, children.end()}; }

void udm::Element::EraseValue(const Element &child)
{
	auto it = std::find_if(children.begin(), children.end(), [&child](const std::pair<std::string, PProperty> &pair) {
		return get_property_type(*pair.second) == udm::Type::Element && get_property_value(*pair.second) == &child;
	});
	if(it == children.end())
		return;
	children.erase(it);
}
