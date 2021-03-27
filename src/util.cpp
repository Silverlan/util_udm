/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "udm.hpp"
#include <lz4.h>

#pragma optimize("",off)
bool udm::does_key_require_quotes(const std::string_view &key)
{
	return key.find_first_of(udm::CONTROL_CHARACTERS.c_str()) != std::string::npos || key.find_first_of(udm::WHITESPACE_CHARACTERS.c_str()) != std::string::npos || key.find(PATH_SEPARATOR) != std::string::npos;
}

bool udm::is_whitespace_character(char c) {return WHITESPACE_CHARACTERS.find(c) != std::string::npos;}
bool udm::is_control_character(char c) {return CONTROL_CHARACTERS.find(c) != std::string::npos;}

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

udm::Reference &udm::Reference::operator=(Reference &&other)
{
	if(this == &other)
		return *this;
	property = other.property;
	path = std::move(other.path);
	static_assert(sizeof(*this) == 48,"Update this function when the struct has changed!");
	return *this;
}
udm::Reference &udm::Reference::operator=(const Reference &other)
{
	if(this == &other)
		return *this;
	property = other.property;
	path = other.path;
	static_assert(sizeof(*this) == 48,"Update this function when the struct has changed!");
	return *this;
}

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
#pragma optimize("",on)
