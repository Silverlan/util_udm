/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>
#pragma optimize("",off)
udm::Blob udm::decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize)
{
	udm::Blob dst {};
	dst.data.resize(uncompressedSize);
	auto size = LZ4_decompress_safe(reinterpret_cast<const char*>(compressedData),reinterpret_cast<char*>(dst.data.data()),compressedSize,uncompressedSize);
	if(size < 0)
		throw CompressionError{"Unable to decompress LZ4 blob data buffer of size " +std::to_string(compressedSize)};
	if(size != uncompressedSize)
		throw CompressionError{"LZ4 blob data decompression size mismatch!"};
	return dst;
}

udm::Blob udm::decompress_lz4_blob(const BlobLz4 &data)
{
	return decompress_lz4_blob(data.compressedData.data(),data.compressedData.size(),data.uncompressedSize);
}

udm::BlobLz4 udm::compress_lz4_blob(const void *data,uint64_t srcSize)
{
	udm::BlobLz4 compressed {};
	compressed.uncompressedSize = srcSize;
	compressed.compressedData.resize(LZ4_compressBound(srcSize));
	auto size = LZ4_compress_default(reinterpret_cast<const char*>(data),reinterpret_cast<char*>(compressed.compressedData.data()),srcSize,compressed.compressedData.size() *sizeof(compressed.compressedData.front()));
	if(size < 0)
		throw CompressionError{"Unable to compress blob data buffer of size " +std::to_string(srcSize)};
	compressed.compressedData.resize(size);
	return compressed;
}

udm::BlobLz4 udm::compress_lz4_blob(const Blob &data) {return compress_lz4_blob(data.data.data(),data.data.size());}

udm::PProperty udm::Property::Create(Type type)
{
	auto prop = std::shared_ptr<Property>{new Property{}};
	prop->type = type;
	prop->Initialize();
	if(type == Type::Element)
		static_cast<Element*>(prop->value)->fromProperty = *prop;
	else if(type == Type::Array)
		static_cast<Array*>(prop->value)->fromProperty = *prop;
	return prop;
}

udm::Property::Property(Property &&other)
{
	type = other.type;
	value = other.value;

	other.type = Type::Nil;
	other.value = nullptr;
}

udm::Property::~Property() {Clear();}

void udm::Property::Initialize()
{
	Clear();
	if(is_non_trivial_type(type))
	{
		auto tag = get_non_trivial_tag(type);
		std::visit([&](auto tag){value = new decltype(tag)::type{};},tag);
		return;
	}
	if(type == Type::Nil)
		return;
	auto size = size_of(type);
	value = new uint8_t[size];
}

void udm::Property::Clear()
{
	if(value == nullptr)
		return;
	if(is_trivial_type(type))
		delete[] static_cast<uint8_t*>(value);
	else
	{
		if(is_non_trivial_type(type))
		{
			auto tag = get_non_trivial_tag(type);
			std::visit([&](auto tag){delete static_cast<decltype(tag)::type*>(this->value);},tag);
		}
	}
	value = nullptr;
}

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
			if(values)
			{
				auto numCpy = umath::min(newSize,size);
				for(auto i=decltype(numCpy){0u};i<numCpy;++i)
					newValues[i] = static_cast<T*>(values)[i];
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
}

void udm::PropertyWrapper::operator=(const PropertyWrapper &other) {operator=(const_cast<PropertyWrapper&>(other));}
void udm::PropertyWrapper::operator=(PropertyWrapper &other)
{
	prop = other.prop;
	arrayIndex = other.arrayIndex;
	static_assert(sizeof(PropertyWrapper) == 16,"Update this function when the struct has changed!");
}
void udm::PropertyWrapper::operator=(PropertyWrapper &&other)
{
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
		return prop && prop->type != Type::Nil;
	auto *a = GetOwningArray();
	if(a == nullptr || !linked)
		return false;
	auto &linkedWrapper = static_cast<const LinkedPropertyWrapper&>(*this);
	return linkedWrapper.propName.empty() || static_cast<bool>(const_cast<Element&>(a->GetValue<Element>(arrayIndex)).children[linkedWrapper.propName]);
}

void *udm::PropertyWrapper::GetValuePtr(Type &outType)
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetValuePtr(outType);
		outType = a.valueType;
		return static_cast<uint8_t*>(a.values) +arrayIndex *size_of(a.valueType);
	}
	return (*this)->GetValuePtr(outType);
}

bool udm::PropertyWrapper::IsArrayItem() const
{
	/*return arrayIndex != std::numeric_limits<uint32_t>::max() && linked && 
		static_cast<const LinkedPropertyWrapper&>(*this).prev &&
		static_cast<const LinkedPropertyWrapper&>(*this).prev->prop &&
		static_cast<const LinkedPropertyWrapper&>(*this).prev->prop->IsType(Type::Array);*/
	return arrayIndex != std::numeric_limits<uint32_t>::max() && prop && prop->IsType(Type::Array);
}

udm::Array *udm::PropertyWrapper::GetOwningArray()
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
		switch(a.valueType)
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
			a.IsValueType(Type::BlobLz4) ? Property::GetBlobData(a.GetValue<BlobLz4>(arrayIndex)) :
			udm::Blob{};
	}
	return (*this)->GetBlobData(outType);
}

uint32_t udm::PropertyWrapper::GetSize() const
{
	return (static_cast<bool>(*this) && (*this)->IsType(Type::Array)) ? GetValue<udm::Array>().GetSize() : 0;
}
void udm::PropertyWrapper::Resize(uint32_t size)
{
	if(!static_cast<bool>(*this) || (*this)->IsType(Type::Array) == false)
		return;
	GetValue<udm::Array>().Resize(size);
}
udm::ArrayIterator<udm::Element> udm::PropertyWrapper::begin() {return begin<Element>();}
udm::ArrayIterator<udm::Element> udm::PropertyWrapper::end() {return end<Element>();}

uint32_t udm::PropertyWrapper::GetChildCount() const
{
	auto *e = GetValuePtr<Element>();
	return e ? e->children.size() : 0;
}
udm::ElementIterator udm::PropertyWrapper::begin_el()
{
	if(!static_cast<bool>(*this))
		return ElementIterator{};
	auto *e = GetValuePtr<Element>();
	if(e == nullptr)
		return ElementIterator{};
	return e->begin();
}
udm::ElementIterator udm::PropertyWrapper::end_el()
{
	if(!static_cast<bool>(*this))
		return ElementIterator{};
	auto *e = GetValuePtr<Element>();
	if(e == nullptr)
		return ElementIterator{};
	return e->end();
}
udm::ElementIteratorWrapper udm::PropertyWrapper::ElIt() {return ElementIteratorWrapper{*this};}

udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,std::optional<uint32_t> size,Type type)
{
	if(arrayIndex != std::numeric_limits<uint32_t>::max())
	{
		if(IsArrayItem())
		{
			auto &a = *static_cast<Array*>(prop->value);
			if(a.valueType == Type::Element)
			{
				auto *e = &a.GetValue<Element>(arrayIndex);
				if(linked)
				{
					auto &linkedWrapper = static_cast<LinkedPropertyWrapper&>(*this);
					if(linkedWrapper.propName.empty() == false)
					{
						auto child = (*e)[linkedWrapper.propName];
						e = child.GetValuePtr<Element>();
					}
				}
				if(!e)
					throw InvalidUsageError{"Attempted to add key-value to invalid array element!"};
				auto wrapper = e->AddArray(path,size,type);
				static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
				return wrapper;
			}
		}
		if(linked)
		{
			// TODO: Is this obsolete?
			auto &linkedWrapper = static_cast<LinkedPropertyWrapper&>(*this);
			if(linkedWrapper.prev && linkedWrapper.prev->prop && linkedWrapper.prev->prop->type == Type::Array)
			{
				auto &a = *static_cast<Array*>(linkedWrapper.prev->prop->value);
				if(a.valueType == Type::Element)
				{
					auto wrapper = a.GetValue<Element>(arrayIndex).AddArray(path,size,type);
					static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
					return wrapper;
				}
			}
		}
		throw InvalidUsageError{"Attempted to add key-value to indexed property with invalid array reference!"};
	}
	if(prop == nullptr && linked)
		static_cast<udm::LinkedPropertyWrapper&>(*this).InitializeProperty();
	if(prop == nullptr || prop->type != Type::Element)
		throw InvalidUsageError{"Attempted to add key-value to non-element property of type " +std::string{magic_enum::enum_name(prop->type)} +", which is not allowed!"};
	auto wrapper = static_cast<Element*>(prop->value)->AddArray(path,size,type);
	static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
	return wrapper;
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::Add(const std::string_view &path,Type type)
{
	if(arrayIndex != std::numeric_limits<uint32_t>::max())
	{
		if(linked)
		{
			auto &linkedWrapper = static_cast<LinkedPropertyWrapper&>(*this);
			if(linkedWrapper.prev && linkedWrapper.prev->prop && linkedWrapper.prev->prop->type == Type::Array)
			{
				auto &a = *static_cast<Array*>(linkedWrapper.prev->prop->value);
				if(a.valueType == Type::Element)
				{
					auto wrapper = a.GetValue<Element>(arrayIndex).Add(path,type);
					static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
					return wrapper;
				}
			}
		}
		throw InvalidUsageError{"Attempted to add key-value to indexed property with invalid array reference!"};
	}
	if(prop == nullptr && linked)
		static_cast<udm::LinkedPropertyWrapper&>(*this).InitializeProperty();
	if(prop == nullptr || prop->type != Type::Element)
		throw InvalidUsageError{"Attempted to add key-value to non-element property of type " +std::string{magic_enum::enum_name(prop->type)} +", which is not allowed!"};
	auto wrapper = static_cast<Element*>(prop->value)->Add(path,type);
	static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
	return wrapper;
}

udm::LinkedPropertyWrapper::LinkedPropertyWrapper(const LinkedPropertyWrapper &other)
	: PropertyWrapper{other},propName{other.propName},prev{other.prev ? std::make_unique<LinkedPropertyWrapper>(*other.prev) : nullptr}
{
	linked = true;
}

void udm::Element::AddChild(const std::string &key,const PProperty &o)
{
	children[key] = o;
	if(o->type == Type::Element)
	{
		auto *el = static_cast<Element*>(o->value);
		el->parentProperty = fromProperty;
		el->fromProperty = *o;
	}
	else if(o->type == Type::Array)
		static_cast<Array*>(o->value)->fromProperty = *o;
}

udm::Property *udm::PropertyWrapper::operator*() {return prop;}

udm::LinkedPropertyWrapper udm::PropertyWrapper::Prop(const std::string_view &key) const
{
	auto sep = key.find('.');
	if(sep != std::string::npos)
		return Prop(key.substr(0,sep)).Prop(key.substr(sep +1));
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
	if(is_numeric_type(a->valueType))
		return std::visit(vs,get_numeric_tag(a->valueType));
	else if(is_generic_type(a->valueType))
		return std::visit(vs,get_generic_tag(a->valueType));
	else if(is_non_trivial_type(a->valueType))
		return std::visit(vs,get_non_trivial_tag(a->valueType));
	return {};
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
	else
	{
		if(arrayIndex == std::numeric_limits<uint32_t>::max())
			return {};
		if(linked && static_cast<const LinkedPropertyWrapper&>(*this).propName.empty() == false)
		{
			el = &static_cast<Array*>(prop->value)->GetValue<Element>(arrayIndex);
			auto prop = getElementProperty(*this,*el,static_cast<const LinkedPropertyWrapper&>(*this).propName);
			prop.InitializeProperty(); // TODO: Don't initialize if this is used as a getter
			el = &prop.GetValue<Element>();
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

void udm::LinkedPropertyWrapper::InitializeProperty(Type type,bool getOnly)
{
	assert(type == Type::Element || type == Type::Array);
	auto isArrayElement = (arrayIndex != std::numeric_limits<uint32_t>::max());
	if(prop || prev == nullptr || (propName.empty() && !isArrayElement))
		return;
	prev->InitializeProperty(!isArrayElement ? Type::Element : Type::Array);
	if(prev->prop == nullptr || prev->prop->type != Type::Element)
	{
		if(prev->prop && prev->prop->type == Type::Array && prev->arrayIndex != std::numeric_limits<uint32_t>::max() && static_cast<Array*>(prev->prop->value)->IsValueType(Type::Element))
		{
			prop = static_cast<Array*>(prev->prop->value)->GetValue<Element>(prev->arrayIndex).Add(propName,!isArrayElement ? Type::Element : Type::Array).prop;
			return;
		}
		if(isArrayElement && prev->prop && prev->prop->type == Type::Array)
			prop = (*static_cast<Array*>(prev->prop->value))[arrayIndex].prop;
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

bool udm::PropertyWrapper::operator==(const PropertyWrapper &other) const
{
	if(linked != other.linked)
		return false;
	if(linked)
		return static_cast<const udm::LinkedPropertyWrapper&>(*this) == static_cast<const udm::LinkedPropertyWrapper&>(other);
	return prop == other.prop && arrayIndex == other.arrayIndex;
}
bool udm::PropertyWrapper::operator!=(const PropertyWrapper &other) const {return !operator==(other);}

bool udm::LinkedPropertyWrapper::operator==(const LinkedPropertyWrapper &other) const
{
	return prop == other.prop && arrayIndex == other.arrayIndex && propName == other.propName;
}
bool udm::LinkedPropertyWrapper::operator!=(const LinkedPropertyWrapper &other) const {return !operator==(other);}

udm::ElementIteratorPair::ElementIteratorPair(std::unordered_map<std::string,PProperty>::iterator &it)
	: key{it->first},property{*it->second}
{}
udm::ElementIteratorPair::ElementIteratorPair()=default;
bool udm::ElementIteratorPair::operator==(const ElementIteratorPair &other) const
{
	return key == other.key && property == other.property;
}
bool udm::ElementIteratorPair::operator!=(const ElementIteratorPair &other) const {return !operator==(other);}

udm::ElementIteratorWrapper::ElementIteratorWrapper(PropertyWrapper &prop)
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

udm::ElementIterator udm::Element::begin()
{
	return ElementIterator{*this,children.begin()};
}
udm::ElementIterator udm::Element::end()
{
	return ElementIterator{*this,children.end()};
}
#pragma optimize("",on)
