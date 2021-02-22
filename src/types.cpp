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
		throw std::runtime_error{"Unable to decompress LZ4 blob data buffer of size " +std::to_string(compressedSize)};
	if(size != uncompressedSize)
		throw std::runtime_error{"LZ4 blob data decompression size mismatch!"};
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
		throw std::runtime_error{"Unable to compress blob data buffer of size " +std::to_string(srcSize)};
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

void udm::Array::SetValueType(Type valueType)
{
	if(valueType == this->valueType)
		return;
	Clear();
	this->valueType = valueType;
	Resize(size);
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
				if(newSize > size)
					memset(newValues +size,0,(newSize -size) *sizeof(T));
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
	if(arrayIndex == std::numeric_limits<uint32_t>::max())
		return prop;
	auto *a = GetOwningArray();
	if(a == nullptr || !linked)
		return false;
	auto &linkedWrapper = static_cast<const LinkedPropertyWrapper&>(*this);
	return linkedWrapper.propName.empty() || static_cast<bool>(const_cast<Element&>(a->GetValue<Element>(arrayIndex)).children[linkedWrapper.propName]);
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

bool udm::PropertyWrapper::GetBlobData(void *outBuffer,size_t bufferSize) const
{
	if(IsArrayItem())
	{
		auto &a = *GetOwningArray();
		if(linked && !static_cast<const LinkedPropertyWrapper&>(*this).propName.empty())
			return const_cast<Element&>(a.GetValue<Element>(arrayIndex)).children[static_cast<const LinkedPropertyWrapper&>(*this).propName]->GetBlobData(outBuffer,bufferSize);
		return a.IsValueType(Type::Blob) ? Property::GetBlobData(a.GetValue<Blob>(arrayIndex),outBuffer,bufferSize) :
			a.IsValueType(Type::BlobLz4) ? Property::GetBlobData(a.GetValue<BlobLz4>(arrayIndex),outBuffer,bufferSize) :
			false;
	}
	return (*this)->GetBlobData(outBuffer,bufferSize);
}

uint32_t udm::PropertyWrapper::GetSize() const
{
	return (static_cast<bool>(*this) && (*this)->IsType(Type::Array)) ? GetValue<udm::Array>().GetSize() : 0;
}
udm::ArrayIterator<udm::Element> udm::PropertyWrapper::begin() {return begin<Element>();}
udm::ArrayIterator<udm::Element> udm::PropertyWrapper::end() {return end<Element>();}

udm::LinkedPropertyWrapper udm::PropertyWrapper::AddArray(const std::string_view &path,std::optional<uint32_t> size,Type type)
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
					auto wrapper = a.GetValue<Element>(arrayIndex).AddArray(path,size,type);
					static_cast<LinkedPropertyWrapper&>(wrapper).prev = std::make_unique<LinkedPropertyWrapper>(*this);
					return wrapper;
				}
			}
		}
		throw std::logic_error{"Attempted to add key-value to indexed property with invalid array reference!"};
	}
	if(prop == nullptr || prop->type != Type::Element)
		throw std::logic_error{"Attempted to add key-value to non-element property of type " +std::string{magic_enum::enum_name(prop->type)} +", which is not allowed!"};
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
		throw std::logic_error{"Attempted to add key-value to indexed property with invalid array reference!"};
	}
	if(prop == nullptr || prop->type != Type::Element)
		throw std::logic_error{"Attempted to add key-value to non-element property of type " +std::string{magic_enum::enum_name(prop->type)} +", which is not allowed!"};
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

udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](uint32_t idx) const
{
	udm::LinkedPropertyWrapper wrapper {};
	wrapper.prev = linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(*this)) : std::make_unique<udm::LinkedPropertyWrapper>(*this);
	wrapper.arrayIndex = idx;
	return wrapper;
}

udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](const char *key) const {return operator[](std::string{key});}
udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](int32_t idx) const {return operator[](static_cast<uint32_t>(idx));}
udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](size_t idx) const {return operator[](static_cast<uint32_t>(idx));}

udm::LinkedPropertyWrapper udm::PropertyWrapper::operator[](const std::string &key) const
{
	auto sep = key.find('.');
	if(sep != std::string::npos)
		return (*this)[key.substr(0,sep)][key.substr(sep +1)];
	if(prop == nullptr)
	{
		udm::LinkedPropertyWrapper wrapper {};
		wrapper.prev = linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(*this)) : std::make_unique<udm::LinkedPropertyWrapper>(*this);
		wrapper.propName = key;
		return wrapper;
	}
	switch(prop->type)
	{
	case Type::Element:
	{
		auto *el = static_cast<Element*>(prop->value);
		auto it = el->children.find(key);
		if(it == el->children.end())
		{
			udm::LinkedPropertyWrapper wrapper {};
			wrapper.prev = linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(*this)) : std::make_unique<udm::LinkedPropertyWrapper>(*this);
			wrapper.propName = key;
			return wrapper;
		}
		udm::LinkedPropertyWrapper wrapper {*it->second};
		wrapper.prev = linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(*this)) : std::make_unique<udm::LinkedPropertyWrapper>(*this);
		wrapper.propName = key;
		return wrapper;
	}
	case Type::Array:
	{
		if(arrayIndex == std::numeric_limits<uint32_t>::max())
			return {};
		auto el = (*static_cast<Array*>(prop->value))[arrayIndex];
		udm::LinkedPropertyWrapper wrapper {el};
		wrapper.prev = linked ? std::make_unique<udm::LinkedPropertyWrapper>(static_cast<const LinkedPropertyWrapper&>(*this)) : std::make_unique<udm::LinkedPropertyWrapper>(*this);
		wrapper.propName = key;
		return wrapper;
	}
	}
	return {};
}

void udm::LinkedPropertyWrapper::InitializeProperty(Type type)
{
	assert(type == Type::Element || type == Type::Array);
	auto isArrayElement = (arrayIndex != std::numeric_limits<uint32_t>::max());
	if(prop || prev == nullptr || (propName.empty() && !isArrayElement))
		return;
	prev->InitializeProperty(!isArrayElement ? Type::Element : Type::Array);
	if(prev->prop == nullptr || prev->prop->type != Type::Element)
		return;
	auto &el = *static_cast<Element*>(prev->prop->value);
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
#pragma optimize("",on)
