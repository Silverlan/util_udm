/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>
#pragma optimize("",off)
udm::Array::~Array() {Clear();}

bool udm::Array::operator==(const Array &other) const
{
	if(size != other.size || valueType != other.valueType)
		return false;
	if(is_trivial_type(valueType))
	{
		auto sz = size_of(valueType);
		return memcmp(values,other.values,sz *size);
	}
	auto tag = get_non_trivial_tag(valueType);
	return std::visit([this,&other](auto tag) -> bool {
		using T = decltype(tag)::type;
		auto *ptr0 = static_cast<T*>(values);
		auto *ptr1 = static_cast<T*>(other.values);
		for(auto i=decltype(size){0u};i<size;++i)
		{
			if(*ptr0 != *ptr1)
				return false;
			++ptr0;
			++ptr1;
		}
		return true;
	},tag);
}

udm::Array &udm::Array::operator=(Array &&other)
{
	if(this == &other)
		return *this;
	Clear();
	valueType = other.valueType;
	size = other.size;
	values = other.values;
	fromProperty = other.fromProperty;

	other.values = nullptr;
	other.size = 0;
	other.valueType = Type::Nil;
	static_assert(sizeof(*this) == 32,"Update this function when the struct has changed!");
	return *this;
}
udm::Array &udm::Array::operator=(const Array &other)
{
	if(this == &other)
		return *this;
	Clear();
	SetValueType(other.valueType);
	Merge(other);
	fromProperty = other.fromProperty;
	static_assert(sizeof(*this) == 32,"Update this function when the struct has changed!");
	return *this;
}

void udm::Array::Merge(const Array &other)
{
	if(valueType != other.valueType)
		return;
	auto size = GetSize();
	auto sizeOther = other.GetSize();
	auto offset = size;
	Resize(size +sizeOther);

	if(is_trivial_type(valueType))
	{
		memcpy(GetValuePtr(offset),other.values,size_of(valueType) *sizeOther);
		return;
	}

	std::visit([this,&other,offset,sizeOther](auto tag) {
		using T = decltype(tag)::type;
		auto *ptrDst = static_cast<T*>(GetValuePtr(offset));
		auto *ptrSrc = static_cast<T*>(other.values);
		for(auto i=decltype(sizeOther){0u};i<sizeOther;++i)
			ptrDst[i] = ptrSrc[i];
	},get_non_trivial_tag(valueType));
}

void udm::Array::SetValueType(Type valueType)
{
	if(valueType == this->valueType)
		return;
	Clear();
	this->valueType = valueType;
	Resize(size);
}

void *udm::Array::GetValuePtr(uint32_t idx)
{
	return static_cast<uint8_t*>(values) +idx *size_of_base_type(valueType);
}

void udm::Array::Resize(uint32_t newSize)
{
	if(newSize == size)
		return;
	if(is_non_trivial_type(valueType))
	{
		auto tag = get_non_trivial_tag(valueType);
		return std::visit([this,newSize](auto tag) {
			using T = decltype(tag)::type;
			auto *newValues = new T[newSize];
			//for(auto i=decltype(newSize){0u};i<newSize;++i)
			//	new (&newValues[i]) T{};
			if(values)
			{
				auto numCpy = umath::min(newSize,size);
				for(auto i=decltype(numCpy){0u};i<numCpy;++i)
					newValues[i] = std::move(const_cast<T*>(static_cast<T*>(values))[i]);
				for(auto i=size;i<newSize;++i)
					newValues[i] = {}; // Default initialize
				Clear();
			}
			values = newValues;
			size = newSize;
			
			// Also see udm::Property::Read (with array overload)
			if constexpr(std::is_same_v<T,Element> || std::is_same_v<T,Array>)
			{
				auto *p = static_cast<T*>(values);
				for(auto i=decltype(size){0u};i<size;++i)
					p[i].fromProperty = PropertyWrapper{*this,i};
			}
		},tag);
		return;
	}

	auto sizeBytes = newSize *size_of(valueType);
	auto *newValues = new uint8_t[sizeBytes];
	if(values)
	{
		auto cpySizeBytes = umath::min(size *size_of(valueType),sizeBytes);
		memcpy(newValues,values,cpySizeBytes);
		if(sizeBytes > cpySizeBytes)
			memset(newValues +cpySizeBytes,0,sizeBytes -cpySizeBytes);
		Clear();
	}
	values = newValues;
	size = newSize;
}

udm::PropertyWrapper udm::Array::operator[](uint32_t idx)
{
	return PropertyWrapper{*this,idx};
}

void udm::Array::Clear()
{
	if(values == nullptr)
		return;
	if(is_trivial_type(valueType))
		delete[] static_cast<uint8_t*>(values);
	else
	{
		auto tag = get_non_trivial_tag(valueType);
		std::visit([&](auto tag){
			using T = decltype(tag)::type;
			delete[] static_cast<T*>(this->values);
		},tag);
	}
	values = nullptr;
	size = 0;
}
#pragma optimize("",on)
