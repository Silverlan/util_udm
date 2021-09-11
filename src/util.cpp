/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>

bool udm::does_key_require_quotes(const std::string_view &key)
{
	return key.find_first_of(udm::CONTROL_CHARACTERS.c_str()) != std::string::npos || key.find_first_of(udm::WHITESPACE_CHARACTERS.c_str()) != std::string::npos || key.find(PATH_SEPARATOR) != std::string::npos;
}

bool udm::is_whitespace_character(char c) {return WHITESPACE_CHARACTERS.find(c) != std::string::npos;}
bool udm::is_control_character(char c) {return CONTROL_CHARACTERS.find(c) != std::string::npos;}

void udm::decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize,void *outData)
{
	if(uncompressedSize == 0)
		return;
	auto size = LZ4_decompress_safe(reinterpret_cast<const char*>(compressedData),reinterpret_cast<char*>(outData),compressedSize,uncompressedSize);
	if(size < 0)
		throw CompressionError{"Unable to decompress LZ4 blob data buffer of size " +std::to_string(compressedSize)};
	if(size != uncompressedSize)
		throw CompressionError{"LZ4 blob data decompression size mismatch!"};
}

udm::Blob udm::decompress_lz4_blob(const void *compressedData,uint64_t compressedSize,uint64_t uncompressedSize)
{
	auto x = sizeof(Array);
	udm::Blob dst {};
	dst.data.resize(uncompressedSize);
	decompress_lz4_blob(compressedData,compressedSize,uncompressedSize,dst.data.data());
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
	if(srcSize == 0)
		return compressed;
	compressed.compressedData.resize(LZ4_compressBound(srcSize));
	auto size = LZ4_compress_default(reinterpret_cast<const char*>(data),reinterpret_cast<char*>(compressed.compressedData.data()),srcSize,compressed.compressedData.size() *sizeof(compressed.compressedData.front()));
	if(size < 0)
		throw CompressionError{"Unable to compress blob data buffer of size " +std::to_string(srcSize)};
	compressed.compressedData.resize(size);
	return compressed;
}

udm::BlobLz4 udm::compress_lz4_blob(const Blob &data) {return compress_lz4_blob(data.data.data(),data.data.size());}

//////////////

udm::Half::Half(float value)
	: value{static_cast<uint16_t>(umath::float32_to_float16_glm(value))}
{}

udm::Half::operator float() const {return umath::float16_to_float32_glm(static_cast<int16_t>(value));}

udm::Half &udm::Half::operator=(float value) {this->value = static_cast<uint16_t>(umath::float32_to_float16_glm(value)); return *this;}
udm::Half &udm::Half::operator=(uint16_t value) {this->value = value; return *this;}

//////////////

udm::Struct::Struct(const StructDescription &desc)
	: description{desc}
{
	UpdateData();
}
udm::Struct::Struct(StructDescription &&desc)
	: description{std::move(desc)}
{
	UpdateData();
}
void udm::Struct::SetDescription(const StructDescription &desc)
{
	description = desc;
	UpdateData();
}
void udm::Struct::SetDescription(StructDescription &&desc)
{
	description = std::move(desc);
	UpdateData();
}
void udm::Struct::UpdateData()
{
	data.resize(description.GetDataSizeRequirement());
}
bool udm::Struct::operator==(const Struct &other) const
{
	static_assert(sizeof(*this) == 72,"Update this function when the struct has changed!");
	return description == other.description && data == other.data;
}

bool udm::StructDescription::operator==(const StructDescription &other) const
{
	return types == other.types && names == other.names;
}

void udm::Struct::Clear()
{
	description.Clear();
	data.clear();
}
udm::StructDescription::MemberCountType udm::StructDescription::GetMemberCount() const {return types.size();}
std::string udm::StructDescription::GetTemplateArgumentList() const
{
	assert(!types.empty());
	std::string r;
	static_assert(umath::to_integral(Type::Count) == 36,"Update this when new types are added!");
	constexpr uint32_t MAX_TYPE_NAME_LENGTH = 10; // Max type name length is that of 'stransform'
	auto sz = types.size() *MAX_TYPE_NAME_LENGTH +2 +types.size() -1;
	for(auto &name : names)
	{
		if(name.empty())
			continue;
		sz += name.length() +1;
	}
	r.reserve(sz);
	r += '<';
	for(auto i=decltype(types.size()){0u};i<types.size();++i)
	{
		if(i > 0)
			r += ',';
		r += enum_type_to_ascii(types[i]);
		if(names[i].empty())
			continue;
		r += ':' +names[i];
	}
	r += '>';
	return r;
}
udm::StructDescription::SizeType udm::StructDescription::GetDataSizeRequirement() const
{
	SizeType size = 0;
	for(auto type : types)
		size += size_of(type);
	return size;
}

//////////////

udm::Reference &udm::Reference::operator=(Reference &&other)
{
	if(this == &other)
		return *this;
	property = other.property;
	path = std::move(other.path);
	static_assert(sizeof(*this) == 40,"Update this function when the struct has changed!");
	return *this;
}
udm::Reference &udm::Reference::operator=(const Reference &other)
{
	if(this == &other)
		return *this;
	property = other.property;
	path = other.path;
	static_assert(sizeof(*this) == 40,"Update this function when the struct has changed!");
	return *this;
}
void udm::Reference::InitializeProperty(const LinkedPropertyWrapper &root) {property = root.GetFromPath(path).prop;}

//////////////

udm::Utf8String &udm::Utf8String::operator=(Utf8String &&other)
{
	if(this == &other)
		return *this;
	data = std::move(other.data);
	static_assert(sizeof(*this) == 24,"Update this function when the struct has changed!");
	return *this;
}
udm::Utf8String &udm::Utf8String::operator=(const Utf8String &other)
{
	if(this == &other)
		return *this;
	data = other.data;
	static_assert(sizeof(*this) == 24,"Update this function when the struct has changed!");
	return *this;
}

//////////////

udm::Blob &udm::Blob::operator=(Blob &&other)
{
	if(this == &other)
		return *this;
	data = std::move(other.data);
	static_assert(sizeof(*this) == 24,"Update this function when the struct has changed!");
	return *this;
}
udm::Blob &udm::Blob::operator=(const Blob &other)
{
	if(this == &other)
		return *this;
	data = other.data;
	static_assert(sizeof(*this) == 24,"Update this function when the struct has changed!");
	return *this;
}

//////////////

udm::BlobLz4 &udm::BlobLz4::operator=(BlobLz4 &&other)
{
	if(this == &other)
		return *this;
	uncompressedSize = other.uncompressedSize;
	compressedData = std::move(other.compressedData);
	static_assert(sizeof(*this) == 32,"Update this function when the struct has changed!");
	return *this;
}
udm::BlobLz4 &udm::BlobLz4::operator=(const BlobLz4 &other)
{
	if(this == &other)
		return *this;
	uncompressedSize = other.uncompressedSize;
	compressedData = other.compressedData;
	static_assert(sizeof(*this) == 32,"Update this function when the struct has changed!");
	return *this;
}

//////////////

int32_t udm::IFile::WriteString(const std::string &str)
{
	unsigned int pos = 0;
	while(str[pos] != '\0')
	{
		Write<char>(str[pos]);
		pos++;
	}
	return 0;
}

//////////////

void udm::detail::test_conversions()
{
	auto c = umath::to_integral(udm::Type::Count);
	for(auto i=decltype(c){0u};i<c;++i)
	{
		auto t0 = static_cast<udm::Type>(i);
		for(auto j=decltype(c){0u};j<c;++j)
		{
			auto t1 = static_cast<udm::Type>(j);
			udm::visit(t0,[&](auto tag) {
				using T0 = decltype(tag)::type;
				udm::visit(t1,[&](auto tag) {
					using T1 = decltype(tag)::type;
					if constexpr(!udm::detail::TypeConverter<T0,T1>::is_convertible)
						std::cout<<magic_enum::enum_name(t0)<<" => "<<magic_enum::enum_name(t1)<<": "<<(udm::detail::TypeConverter<T0,T1>::is_convertible ? "defined" : "undefined")<<std::endl;
					else
					{
						T1 value = udm::detail::TypeConverter<T0,T1>::convert(T0{});
					}
				});
			});
			
		}
	}
}
