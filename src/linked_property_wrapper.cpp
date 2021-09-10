/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>
#include <sharedutils/util_string.h>

udm::LinkedPropertyWrapper::LinkedPropertyWrapper(const LinkedPropertyWrapper &other)
	: PropertyWrapper{other},propName{other.propName},prev{other.prev ? std::make_unique<LinkedPropertyWrapper>(*other.prev) : nullptr}
{
	linked = true;
}

void udm::LinkedPropertyWrapper::operator=(PropertyWrapper &&v)
{
	if(this == &v)
		return;
	auto *lp = v.GetLinked();
	if(lp == nullptr)
		return operator=(std::move(*lp));
	PropertyWrapper::operator=(v);
}

void udm::LinkedPropertyWrapper::operator=(LinkedPropertyWrapper &&v)
{
	if(this == &v)
		return;
	PropertyWrapper::operator=(v);
	prev = std::move(v.prev);
	propName = std::move(v.propName);
	static_assert(sizeof(LinkedPropertyWrapper) == 56,"Update this function when the struct has changed!");
}

void udm::LinkedPropertyWrapper::operator=(const PropertyWrapper &v)
{
	if(this == &v)
		return;
	auto *lp = v.GetLinked();
	if(lp == nullptr)
		return operator=(*lp);
	PropertyWrapper::operator=(v);
}
void udm::LinkedPropertyWrapper::operator=(const LinkedPropertyWrapper &v)
{
	if(this == &v)
		return;
	PropertyWrapper::operator=(v);
	prev = v.prev ? std::make_unique<LinkedPropertyWrapper>(*v.prev) : nullptr;
	propName = v.propName;
	static_assert(sizeof(LinkedPropertyWrapper) == 56,"Update this function when the struct has changed!");
}

std::string udm::LinkedPropertyWrapper::GetPath() const
{
	std::string path = propName;
	if(path.empty() && prop && prev && prev->IsType(Type::Element))
	{
		auto &e = prev->GetValue<Element>();
		auto it = std::find_if(e.children.begin(),e.children.end(),[this](const std::pair<std::string,PProperty> &pair) {
			return pair.second.get() == prop;
		});
		if(it != e.children.end())
			path = it->first;
	}
	ustring::replace(path,"/","\\/");
	if(prev)
	{
		auto tmp = prev->GetPath();
		if(!tmp.empty())
		{
			if(IsArrayItem() && propName.empty())
				path = tmp +'[' +std::to_string(arrayIndex) +']';
			else
				path = tmp +PATH_SEPARATOR +path;
		}
	}
	return path;
}

udm::PProperty udm::LinkedPropertyWrapper::ClaimOwnership() const
{
	if(prev == nullptr || IsArrayItem() || prev->IsType(Type::Element) == false)
		return nullptr;
	auto &el = prev->GetValue<Element>();
	auto it = el.children.find(propName);
	return (it != el.children.end()) ? it->second : nullptr;
}

udm::Property *udm::LinkedPropertyWrapper::GetProperty(std::vector<uint32_t> *outArrayIndices) const
{
	if(prop)
	{
		if(arrayIndex != std::numeric_limits<uint32_t>::max() && outArrayIndices)
			outArrayIndices->push_back(arrayIndex);
		return prop;
	}
	if(arrayIndex == std::numeric_limits<uint32_t>::max() || !prev)
		return nullptr;
	if(prev->prop && prev->arrayIndex == std::numeric_limits<uint32_t>::max())
		return nullptr; // Previous prop *has* to be an array element, or something went wrong
	if(outArrayIndices)
		outArrayIndices->push_back(arrayIndex);
	return prev->GetProperty(outArrayIndices);
}

void udm::LinkedPropertyWrapper::InitializeProperty(Type type,bool getOnly)
{
	assert(type == Type::Element || type == Type::Array);
	auto isArrayElement = (arrayIndex != std::numeric_limits<uint32_t>::max());
	if(prop || prev == nullptr || (propName.empty() && !isArrayElement))
		return;
	prev->InitializeProperty(!isArrayElement ? Type::Element : Type::Array,getOnly);
	if(prev->prop == nullptr || prev->prop->type != Type::Element)
	{
		if(prev->prop && prev->prop->type == Type::Array && prev->arrayIndex != std::numeric_limits<uint32_t>::max() && static_cast<Array*>(prev->prop->value)->IsValueType(Type::Element))
		{
			prop = static_cast<Array*>(prev->prop->value)->GetValue<Element>(prev->arrayIndex).Add(propName,!isArrayElement ? Type::Element : Type::Array).prop;
			return;
		}
		if(isArrayElement && prev->prop && prev->prop->type == Type::Array)
		{
			if(prev->arrayIndex == std::numeric_limits<uint32_t>::max())
				prop = (*static_cast<Array*>(prev->prop->value))[arrayIndex].prop;
			return;
		}
		if(propName.empty())
			return;
		std::vector<uint32_t> arrayIndices;
		auto *arrayProp = prev->GetProperty(&arrayIndices);
		if(arrayProp && arrayProp->IsType(Type::Array))
		{
			auto *a = arrayProp->GetValuePtr<Array>();
			if(arrayIndices.size() > 1)
			{
				for(auto it=arrayIndices.rbegin();it!=arrayIndices.rend() -1;++it)
				{
					if(a->GetValueType() != Type::Array)
						return;
					a = static_cast<Array*>(a->GetValuePtr(*it));
				}
			}
			if(!a)
				return;
			if(arrayIndices.empty() || a->IsValueType(Type::Element) == false)
				return;
			auto &children = static_cast<Element*>(a->GetValuePtr(arrayIndices.front()))->children;
			auto it = children.find(propName);
			if(it != children.end())
				prop = it->second.get();
		}
		return;
	}
	auto &el = *static_cast<Element*>(prev->prop->value);
	if(getOnly)
	{
		auto it = el.children.find(propName);
		if(it != el.children.end())
			prop = it->second.get();
		return;
	}
	prop = el.Add(propName,!isArrayElement ? Type::Element : Type::Array).prop;
}

bool udm::LinkedPropertyWrapper::operator==(const LinkedPropertyWrapper &other) const
{
	auto res = (prop == other.prop && arrayIndex == other.arrayIndex && propName == other.propName);
	// UDM_ASSERT_COMPARISON(res);
	return res;
}
bool udm::LinkedPropertyWrapper::operator!=(const LinkedPropertyWrapper &other) const {return !operator==(other);}

udm::ElementIteratorWrapper udm::LinkedPropertyWrapper::ElIt()
{
	return ElementIteratorWrapper{*this};
	//if(linked)
	//	return ElementIteratorWrapper{*static_cast<LinkedPropertyWrapper*>(this)};
	//return ElementIteratorWrapper{LinkedPropertyWrapper{*this}};
}
