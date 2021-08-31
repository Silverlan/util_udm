/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_TYPES_HPP__
#define __UDM_TYPES_HPP__

#include <cinttypes>
#include <memory>

namespace udm
{
	struct Half;
	struct Exception;
	struct InvalidUsageError;
	struct CompressionError;
	struct FileError;
	struct InvalidFormatError;
	struct PropertyLoadError;
	struct OutOfBoundsError;
	struct ImplementationError;
	struct LogicError;
	struct ComparisonError;
	struct AsciiException;
	struct SyntaxError;
	struct DataError;
	struct Blob;
	struct BlobLz4;
	struct Utf8String;
	enum class Type : uint8_t;
	enum class ArrayType : uint8_t;
	enum class BlobResult : uint8_t;
	enum class MergeFlags : uint32_t;
	enum class AsciiSaveFlags : uint32_t;
	struct LinkedPropertyWrapper;
	using LinkedPropertyWrapperArg = const LinkedPropertyWrapper&;
	struct Array;
	struct StructDescription;
	struct IFile;
	struct Property;
	using PProperty = std::shared_ptr<Property>;
	using WPProperty = std::weak_ptr<Property>;
	struct Property;
	struct Header;
	struct PropertyWrapper;
	class ElementIterator;
	struct ElementIteratorWrapper;
	class Data;
	struct Reference;
	struct Struct;
	struct Element;
	struct ElementIteratorPair;
	class AsciiReader;
	struct ArrayLz4;
	struct AssetData;
	using AssetDataArg = const AssetData&;
	enum class FormatType : uint8_t;
	enum class AsciiSaveFlags : uint32_t;
	struct IFile;
	struct MemoryFile;
	struct VectorFile;
	struct VFilePtr;
	class Data;
};

#endif
