/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <sstream>


udm::LinkedPropertyWrapper udm::Element::AddArray(const std::string_view &path,std::optional<uint32_t> size,Type type,ArrayType arrayType)
{
	auto prop = Add(path,(arrayType == ArrayType::Compressed) ? Type::ArrayLz4 : Type::Array);
	if(!prop)
		return prop;
	auto &a = *static_cast<Array*>(prop->value);
	a.SetValueType(type);
	if(size.has_value())
		a.Resize(*size);
	return prop;
}

udm::LinkedPropertyWrapper udm::Element::Add(const std::string_view &path,Type type)
{
	auto end = std::string::npos; // path.find('.');
	auto name = path.substr(0,end);
	if(name.empty())
		return fromProperty;
	std::string strName {name};
	auto isLast = (end == std::string::npos);
	auto it = children.find(strName);
	if(isLast && it != children.end() && it->second->type != type)
	{
		children.erase(it);
		it = children.end();
	}
	if(it == children.end())
	{
		if(isLast)
			AddChild(strName,Property::Create(type));
		else
			AddChild(strName,Property::Create<Element>());
		it = children.find(strName);
		assert(it != children.end());
	}
	if(isLast)
		return *it->second;
	return static_cast<Element*>(it->second->value)->Add(path.substr(end +1),type);
}

bool udm::Element::operator==(const Element &other) const
{
	for(auto &pair : children)
	{
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
	static_assert(sizeof(*this) == 96,"Update this function when the struct has changed!");
	return *this;
}
udm::Element &udm::Element::operator=(const Element &other)
{
	if(this == &other)
		return *this;
	children = other.children;
	fromProperty = other.fromProperty;
	parentProperty = other.parentProperty;
	static_assert(sizeof(*this) == 96,"Update this function when the struct has changed!");
	return *this;
}

void udm::Element::ToAscii(AsciiSaveFlags flags,std::stringstream &ss,const std::optional<std::string> &prefix) const
{
	auto childPrefix = prefix.has_value() ? (*prefix +'\t') : "";
	auto first = true;
	for(auto &pair : children)
	{
		if(first)
			first = false;
		else
			ss<<"\n";
		pair.second->ToAscii(flags,ss,pair.first,childPrefix);
	}
}

void udm::Element::Merge(const Element &other,MergeFlags mergeFlags)
{
	for(auto &pair : other.children)
	{
		auto &prop = *pair.second;
		if(!prop.IsType(Type::Element) && !is_array_type(prop.type))
		{
			children[pair.first] = pair.second;
			continue;
		}
		auto it = children.find(pair.first);
		if(it == children.end() || (prop.type != it->second->type && (!is_array_type(prop.type) || !is_array_type(it->second->type))))
		{
			if(it != children.end() && umath::is_flag_set(mergeFlags,MergeFlags::OverwriteExisting) == false)
				continue;
			children[pair.first] = pair.second;
			continue;
		}
		if(prop.IsType(Type::Element))
		{
			it->second->GetValue<Element>().Merge(prop.GetValue<Element>(),mergeFlags);
			continue;
		}
		// Array property
		it->second->GetValue<Array>().Merge(prop.GetValue<Array>(),mergeFlags);
	}
}

udm::ElementIterator udm::Element::begin()
{
	return ElementIterator{*this,children.begin()};
}
udm::ElementIterator udm::Element::end()
{
	return ElementIterator{*this,children.end()};
}
