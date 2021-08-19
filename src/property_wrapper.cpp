/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"


udm::PropertyWrapper::PropertyWrapper(Property &o)
	: prop{&o}
{}

udm::PropertyWrapper::PropertyWrapper(const PropertyWrapper &other)
	: prop{other.prop},arrayIndex{other.arrayIndex}
{
	static_assert(sizeof(PropertyWrapper) == 16,"Update this function when the struct has changed!");
}
udm::PropertyWrapper::PropertyWrapper(Array &array,uint32_t idx)
	: PropertyWrapper{array.fromProperty}
{
	arrayIndex = idx;
	if(array.fromProperty.prop && array.fromProperty.prop->GetValuePtr<Array>() != &array)
	{
		// Note: This is a special case where the from-property of the array does not point to the array,
		// which means this is a sub-array of another array (since array items do not have properties themselves).
		// We'll set 'prop' to nullptr, but keep the array index, to indicate that this is a sub-array.
		prop = nullptr;
	}
}

void udm::PropertyWrapper::operator=(const PropertyWrapper &other) {operator=(const_cast<PropertyWrapper&>(other));}
void udm::PropertyWrapper::operator=(PropertyWrapper &other)
{
	if(this == &other)
		return;
	prop = other.prop;
	arrayIndex = other.arrayIndex;
	static_assert(sizeof(PropertyWrapper) == 16,"Update this function when the struct has changed!");
}
void udm::PropertyWrapper::operator=(PropertyWrapper &&other)
{
	if(this == &other)
		return;
	prop = other.prop;
	arrayIndex = other.arrayIndex;
	static_assert(sizeof(PropertyWrapper) == 16,"Update this function when the struct has changed!");
}
void udm::PropertyWrapper::operator=(const LinkedPropertyWrapper &other) {operator=(const_cast<LinkedPropertyWrapper&>(other));}
void udm::PropertyWrapper::operator=(LinkedPropertyWrapper &other) {operator=(static_cast<const PropertyWrapper&>(other));}
void udm::PropertyWrapper::operator=(Property &other)
{
	prop = &other;
	arrayIndex = std::numeric_limits<uint32_t>::max();
}

udm::PropertyWrapper::operator bool() const
{
	if(linked)
		const_cast<LinkedPropertyWrapper&>(static_cast<const LinkedPropertyWrapper&>(*this)).InitializeProperty(Type::Element,true);
	if(arrayIndex == std::numeric_limits<uint32_t>::max())
	{
		auto *tmpProp = prop;
		if(linked && !prop)
			tmpProp = static_cast<const LinkedPropertyWrapper&>(*this).GetProperty();
		return tmpProp && tmpProp->type != Type::Nil;
	}
	auto *a = GetOwningArray();
	if(a == nullptr || !linked)
		return false;
	auto &linkedWrapper = static_cast<const LinkedPropertyWrapper&>(*this);
	return linkedWrapper.propName.empty() || static_cast<bool>(const_cast<Element&>(a->GetValue<Element>(arrayIndex)).children[linkedWrapper.propName]);
}

void *udm::PropertyWrapper::GetValuePtr(Type &outType) const
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValuePtr(outType);
		outType = a.GetValueType();
		return static_cast<uint8_t*>(a.GetValues()) +arrayIndex *size_of_base_type(a.GetValueType());
	}
	return (*this)->GetValuePtr(outType);
}

bool udm::PropertyWrapper::IsArrayItem() const
{
	/*return arrayIndex != std::numeric_limits<uint32_t>::max() && linked && 
		static_cast<const LinkedPropertyWrapper&>(*this).prev &&
		static_cast<const LinkedPropertyWrapper&>(*this).prev->prop &&
		static_cast<const LinkedPropertyWrapper&>(*this).prev->prop->IsType(Type::Array);*/
	if(arrayIndex == std::numeric_limits<uint32_t>::max())
		return false;
	if(prop)
		return is_array_type(prop->type);
	if(!linked)
		return false;
	// May be an array of an array
	auto *propLinked = static_cast<const LinkedPropertyWrapper&>(*this).GetProperty();
	return propLinked && is_array_type(propLinked->type);
}

bool udm::PropertyWrapper::IsType(Type type) const
{
	if(!prop)
		return false;
	if(IsArrayItem())
	{
		if(prop->type != Type::Array)
			return false;
		auto &a = prop->GetValue<Array>();
		if(!linked || static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return a.IsValueType(type);
		if(a.IsValueType(Type::Element) == false)
			return false;
		auto &e = *static_cast<Element*>(a.GetValuePtr(arrayIndex));
		auto it = e.children.find(static_cast<const LinkedPropertyWrapper&>(*this).propName);
		return (it != e.children.end()) ? it->second->type == type : false;
	}
	return prop->IsType(type);
}

udm::Type udm::PropertyWrapper::GetType() const
{
	if(!prop)
		return Type::Nil;
	Type type;
	return const_cast<PropertyWrapper*>(this)->GetValuePtr(type) ? type : Type::Nil;
}

void udm::PropertyWrapper::Merge(const PropertyWrapper &other,MergeFlags mergeFlags) const
{
	if(IsType(Type::Element) && other.IsType(Type::Element))
	{
		auto *e = GetValuePtr<Element>();
		auto *eOther = other.GetValuePtr<Element>();
		if(e && eOther)
			e->Merge(*eOther,mergeFlags);
		return;
	}
	if(is_array_type(GetType()) && is_array_type(other.GetType()))
	{
		auto *a = GetValuePtr<Array>();
		auto *aOther = other.GetValuePtr<Array>();
		if(a && aOther)
			a->Merge(*aOther,mergeFlags);
		return;
	}
}

udm::Array *udm::PropertyWrapper::GetOwningArray() const
{
	if(IsArrayItem() == false)
		return nullptr;
	//return &static_cast<const LinkedPropertyWrapper&>(*this).prev->prop->GetValue<Array>();
	return &prop->GetValue<Array>();
}

udm::BlobResult udm::PropertyWrapper::GetBlobData(void *outBuffer,size_t bufferSize,Type type,uint64_t *optOutRequiredSize) const
{
	auto result = GetBlobData(outBuffer,bufferSize,optOutRequiredSize);
	if(result != BlobResult::NotABlobType)
		return result;
	if(IsArrayItem())
		return BlobResult::NotABlobType;
	return (*this)->GetBlobData(outBuffer,bufferSize,type,optOutRequiredSize);
}

udm::BlobResult udm::PropertyWrapper::GetBlobData(void *outBuffer,size_t bufferSize,uint64_t *optOutRequiredSize) const
{
	if(!*this)
		return BlobResult::InvalidProperty;
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetBlobData(outBuffer,bufferSize,optOutRequiredSize);
		switch(a.GetValueType())
		{
		case Type::Blob:
		{
			auto &blob = a.GetValue<Blob>(arrayIndex);
			if(optOutRequiredSize)
				*optOutRequiredSize = blob.data.size();
			return Property::GetBlobData(blob,outBuffer,bufferSize);
		}
		case Type::BlobLz4:
		{
			auto &blob = a.GetValue<BlobLz4>(arrayIndex);
			if(optOutRequiredSize)
				*optOutRequiredSize = blob.uncompressedSize;
			return Property::GetBlobData(blob,outBuffer,bufferSize);
		}
		}
		return BlobResult::NotABlobType;
	}
	return (*this)->GetBlobData(outBuffer,bufferSize,optOutRequiredSize);
}

udm::Blob udm::PropertyWrapper::GetBlobData(Type &outType) const
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetBlobData(outType);
		return a.IsValueType(Type::Blob) ? a.GetValue<Blob>(arrayIndex) :
			a.IsValueType(Type::BlobLz4) ? Property::GetBlobData(const_cast<const BlobLz4&>(a.GetValue<BlobLz4>(arrayIndex))) :
			udm::Blob{};
	}
	return (*this)->GetBlobData(outType);
}

uint32_t udm::PropertyWrapper::GetSize() const
{
	return (static_cast<bool>(*this) && is_array_type(this->GetType())) ? GetValue<udm::Array>().GetSize() : 0;
}
void udm::PropertyWrapper::Resize(uint32_t size) const
{
	if(!static_cast<bool>(*this) || is_array_type(GetType()) == false)
		return;
	GetValue<udm::Array>().Resize(size);
}
udm::ArrayIterator<udm::LinkedPropertyWrapper> udm::PropertyWrapper::begin() const {return begin<LinkedPropertyWrapper>();}
udm::ArrayIterator<udm::LinkedPropertyWrapper> udm::PropertyWrapper::end() const {return end<LinkedPropertyWrapper>();}

uint32_t udm::PropertyWrapper::GetChildCount() const
{
	auto *e = GetValuePtr<Element>();
	return e ? e->children.size() : 0;
}
udm::ElementIterator udm::PropertyWrapper::begin_el() const
{
	if(!static_cast<bool>(*this))
		return ElementIterator{};
	auto *e = GetValuePtr<Element>();
	if(e == nullptr)
		return ElementIterator{};
	auto it = e->begin();
	if(linked)
		(*it).property.prev = std::make_unique<LinkedPropertyWrapper>(*static_cast<LinkedPropertyWrapper*>(const_cast<PropertyWrapper*>(this)));
	return it;
}
udm::ElementIterator udm::PropertyWrapper::end_el() const
{
	if(!static_cast<bool>(*this))
		return ElementIterator{};
	auto *e = GetValuePtr<Element>();
	if(e == nullptr)
		return ElementIterator{};
	return e->end();
}
udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,StructDescription &&strct,std::optional<uint32_t> size,ArrayType arrayType) const
{
	auto prop = AddArray(path,{},Type::Struct,arrayType);
	auto *a = prop.GetValuePtr<Array>();
	if(a)
	{
		if(size.has_value())
			a->Resize(*size);
		auto *ptr = a->GetStructuredDataInfo();
		if(ptr)
			*ptr = std::move(strct);
	}
	return prop;
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,const StructDescription &strct,std::optional<uint32_t> size,ArrayType arrayType) const
{
	auto prop = AddArray(path,{},Type::Struct,arrayType);
	auto *a = prop.GetValuePtr<Array>();
	if(a)
	{
		if(size.has_value())
			a->Resize(*size);
		auto *ptr = a->GetStructuredDataInfo();
		if(ptr)
			*ptr = strct;
	}
	return prop;
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,std::optional<uint32_t> size,Type type,ArrayType arrayType) const
{
	if(arrayIndex != std::numeric_limits<uint32_t>::max())
	{
		if(IsArrayItem())
		{
			auto &a = *static_cast<Array*>(prop->value);
			if(a.GetValueType() == Type::Element)
			{
				auto *e = &a.GetValue<Element>(arrayIndex);
				if(linked)
				{
					auto &linkedWrapper = static_cast<const LinkedPropertyWrapper&>(*this);
					if(linkedWrapper.propName.empty() == false)
					{
						auto child = (*e)[linkedWrapper.propName];
						e = child.GetValuePtr<Element>();
					}
				}
				if(!e)
					throw InvalidUsageError{"Attempted to add key-value to invalid array element!"};
				auto wrapper = e->AddArray(path,size,type,arrayType);
				static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
				return wrapper;
			}
		}
		if(linked)
		{
			// TODO: Is this obsolete?
			auto &linkedWrapper = static_cast<const LinkedPropertyWrapper&>(*this);
			if(linkedWrapper.prev && linkedWrapper.prev->prop && linkedWrapper.prev->prop->type == Type::Array)
			{
				auto &a = *static_cast<Array*>(linkedWrapper.prev->prop->value);
				if(a.GetValueType() == Type::Element)
				{
					auto wrapper = a.GetValue<Element>(arrayIndex).AddArray(path,size,type,arrayType);
					static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
					return wrapper;
				}
			}
		}
		throw InvalidUsageError{"Attempted to add key-value to indexed property with invalid array reference!"};
	}
	if(prop == nullptr && linked)
		const_cast<udm::LinkedPropertyWrapper&>(static_cast<const udm::LinkedPropertyWrapper&>(*this)).InitializeProperty();
	if(prop == nullptr || prop->type != Type::Element)
		throw InvalidUsageError{"Attempted to add key-value to non-element property of type " +std::string{magic_enum::enum_name(prop->type)} +", which is not allowed!"};
	auto wrapper = static_cast<Element*>(prop->value)->AddArray(path,size,type,arrayType);
	static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
	return wrapper;
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::Add(const std::string_view &path,Type type) const
{
	if(arrayIndex != std::numeric_limits<uint32_t>::max())
	{
		if(linked)
		{
			auto &linkedWrapper = static_cast<const LinkedPropertyWrapper&>(*this);
			if(prop && prop->type == Type::Array)
			{
				auto &a = *static_cast<Array*>(prop->value);
				if(a.GetValueType() == Type::Element)
				{
					auto wrapper = a.GetValue<Element>(arrayIndex).Add(path,type);
					static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
					return wrapper;
				}
			}
			/*auto &linkedWrapper = static_cast<LinkedPropertyWrapper&>(*this);
			if(linkedWrapper.prev && linkedWrapper.prev->prop && linkedWrapper.prev->prop->type == Type::Array)
			{
				auto &a = *static_cast<Array*>(linkedWrapper.prev->prop->value);
				if(a.valueType == Type::Element)
				{
					auto wrapper = a.GetValue<Element>(arrayIndex).Add(path,type);
					static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
					return wrapper;
				}
			}*/
		}
		throw InvalidUsageError{"Attempted to add key-value to indexed property with invalid array reference!"};
	}
	if(prop == nullptr && linked)
		const_cast<udm::LinkedPropertyWrapper&>(static_cast<const udm::LinkedPropertyWrapper&>(*this)).InitializeProperty();
	if(prop == nullptr || prop->type != Type::Element)
		throw InvalidUsageError{"Attempted to add key-value to non-element property of type " +std::string{magic_enum::enum_name(prop->type)} +", which is not allowed!"};
	auto wrapper = static_cast<Element*>(prop->value)->Add(path,type);
	static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
	return wrapper;
}

void udm::Element::AddChild(std::string &&key,const PProperty &o)
{
	children[std::move(key)] = o;
	if(o->type == Type::Element)
	{
		auto *el = static_cast<Element*>(o->value);
		el->parentProperty = fromProperty;
		el->fromProperty = *o;
	}
	else if(o->type == Type::Array)
		static_cast<Array*>(o->value)->fromProperty = *o;
}

void udm::Element::AddChild(const std::string &key,const PProperty &o)
{
	auto cpy = key;
	AddChild(std::move(cpy),o);
}

udm::Property *udm::PropertyWrapper::operator*() const {return prop;}

udm::LinkedPropertyWrapper udm::PropertyWrapper::GetFromPath(const std::string_view &key) const
{
	if(key.empty())
		return {};
	auto end = key.find(PATH_SEPARATOR);
	auto baseKeyName = key.substr(0,end);
	std::vector<uint32_t> arrayIndices {};
	while(baseKeyName.back() == ']')
	{
		auto st = baseKeyName.rfind('[');
		if(st == std::string::npos)
			return {};
		auto arrayIndex = util::to_uint(std::string{baseKeyName.substr(st +1,baseKeyName.length() -st -2)});
		arrayIndices.push_back(arrayIndex);
		baseKeyName = baseKeyName.substr(0,st);
	}
	if(baseKeyName.front() == '\"')
	{
		uint32_t idx = 0;
		while(idx < baseKeyName.length())
		{
			if(baseKeyName[idx] == '\"')
				break;
			++idx;
		}
		if(idx >= baseKeyName.length())
			return {};
		baseKeyName = baseKeyName.substr(1,idx -1);
	}
	auto prop = (*this)[baseKeyName];
	for(auto it=arrayIndices.rbegin();it!=arrayIndices.rend();++it)
		prop = prop[*it];
	if(end != std::string::npos)
		return prop.GetFromPath(key.substr(end +1));
	return prop;
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](uint32_t idx) const
{
	auto *a = GetValuePtr<Array>();
	if(a == nullptr)
		return {};
	auto vs = [this,a,idx](auto tag) {
		using TTag = decltype(tag)::type;
		auto it = const_cast<PropertyWrapper*>(this)->begin<TTag>() +idx;
		return it.GetProperty();
	};
	auto valueType = a->GetValueType();
	return visit(valueType,vs);
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](const char *key) const {return operator[](std::string{key});}
udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](int32_t idx) const {return operator[](static_cast<uint32_t>(idx));}
udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](size_t idx) const {return operator[](static_cast<uint32_t>(idx));}

udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](const std::string_view &key) const
{
	if(prop == nullptr)
	{
		udm::LinkedPropertyWrapper wrapper {};
		wrapper.prev = linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(*this)) : std::make_unique<udm::LinkedPropertyWrapper>(*this);
		wrapper.propName = key;
		return wrapper;
	}
	auto getElementProperty = [](const PropertyWrapper &prop,Element &el,const std::string_view &key) {
		auto it = el.children.find(std::string{key});
		if(it == el.children.end())
		{
			udm::LinkedPropertyWrapper wrapper {};
			wrapper.prev = prop.linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(prop)) : std::make_unique<udm::LinkedPropertyWrapper>(prop);
			wrapper.propName = key;
			return wrapper;
		}
		udm::LinkedPropertyWrapper wrapper {*it->second};
		wrapper.prev = prop.linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(prop)) : std::make_unique<udm::LinkedPropertyWrapper>(prop);
		wrapper.propName = key;
		return wrapper;
	};
	Element *el = nullptr;
	if(prop->type == Type::Element)
		el = static_cast<Element*>(prop->value);
	else if(prop->type == Type::Reference)
	{
		auto &ref = *static_cast<Reference*>(prop->value);
		el = ref.property ? ref.property->GetValuePtr<Element>() : nullptr;
	}
	else
	{
		if(arrayIndex == std::numeric_limits<uint32_t>::max())
			return {};
		if(linked && static_cast<const LinkedPropertyWrapper&>(*this).propName.empty() == false)
		{
			el = &static_cast<Array*>(prop->value)->GetValue<Element>(arrayIndex);
			auto prop = getElementProperty(*this,*el,static_cast<const LinkedPropertyWrapper&>(*this).propName);
			prop.InitializeProperty(); // TODO: Don't initialize if this is used as a getter
			el = prop.GetValuePtr<Element>();
			if(el == nullptr)
				return {};
			return getElementProperty(prop,*el,key);
		}
		else
		{
			LinkedPropertyWrapper el = (*static_cast<Array*>(prop->value))[arrayIndex];
			udm::LinkedPropertyWrapper wrapper {el};
			wrapper.prev = linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(*this)) : std::make_unique<udm::LinkedPropertyWrapper>(*this);
			wrapper.propName = key;
			return wrapper;
		}
	}
	if(el == nullptr)
		return {};
	return getElementProperty(*this,*el,key);
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](const std::string &key) const {return operator[](std::string_view{key});}

bool udm::PropertyWrapper::operator==(const PropertyWrapper &other) const
{
	auto res = (linked == other.linked);
	UDM_ASSERT_COMPARISON(res);
	if(!res)
		return false;
	if(linked)
	{
		res = (static_cast<const udm::LinkedPropertyWrapper&>(*this) == static_cast<const udm::LinkedPropertyWrapper&>(other));
		UDM_ASSERT_COMPARISON(res);
		return res;
	}
	res = prop == other.prop && arrayIndex == other.arrayIndex;
	UDM_ASSERT_COMPARISON(res);
	return res;
}
bool udm::PropertyWrapper::operator!=(const PropertyWrapper &other) const {return !operator==(other);}

udm::LinkedPropertyWrapper *udm::PropertyWrapper::GetLinked() {return linked ? static_cast<LinkedPropertyWrapper*>(this) : nullptr;}
