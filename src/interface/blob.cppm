// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.udm:types.blob;

import :exception;

export {
	namespace udm {
		struct DLLUDM Blob {
			static constexpr std::uint32_t layout_version = 1; // Increment this whenever members of this class are changed

			Blob() = default;
			Blob(const Blob &) = default;
			Blob(Blob &&) = default;
			Blob(std::vector<uint8_t> &&data) : data {data} {}
			std::vector<uint8_t> data;

			Blob &operator=(Blob &&other);
			Blob &operator=(const Blob &other);

			bool operator==(const Blob &other) const
			{
				auto res = (data == other.data);
				UDM_ASSERT_COMPARISON(res);
				return res;
			}
			bool operator!=(const Blob &other) const { return !operator==(other); }
		};

		struct DLLUDM BlobLz4 {
			static constexpr std::uint32_t layout_version = 1; // Increment this whenever members of this class are changed
			
			BlobLz4() = default;
			BlobLz4(const BlobLz4 &) = default;
			BlobLz4(BlobLz4 &&) = default;
			BlobLz4(std::vector<uint8_t> &&compressedData, size_t uncompressedSize) : compressedData {compressedData}, uncompressedSize {uncompressedSize} {}
			size_t uncompressedSize = 0;
			std::vector<uint8_t> compressedData;

			BlobLz4 &operator=(BlobLz4 &&other);
			BlobLz4 &operator=(const BlobLz4 &other);

			bool operator==(const BlobLz4 &other) const
			{
				auto res = (uncompressedSize == other.uncompressedSize && compressedData == other.compressedData);
				UDM_ASSERT_COMPARISON(res);
				return res;
			}
			bool operator!=(const BlobLz4 &other) const { return !operator==(other); }
		};

		DLLUDM Blob decompress_lz4_blob(const BlobLz4 &data);
		DLLUDM Blob decompress_lz4_blob(const void *compressedData, uint64_t compressedSize, uint64_t uncompressedSize);
		DLLUDM void decompress_lz4_blob(const void *compressedData, uint64_t compressedSize, uint64_t uncompressedSize, void *outData);
		DLLUDM BlobLz4 compress_lz4_blob(const Blob &data);
		DLLUDM BlobLz4 compress_lz4_blob(const void *data, uint64_t size);
		template<class T>
		BlobLz4 compress_lz4_blob(const T &v)
		{
			return compress_lz4_blob(v.data(), v.size() * sizeof(v[0]));
		}
	}
}
