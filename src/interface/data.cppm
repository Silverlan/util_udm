// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include "sharedutils/magic_enum.hpp"
#include <string>
#include <memory>

export module pragma.udm:data;

export import :core;
export import :enums;
export import :file;
import :property;
export import :types;

export {
	namespace udm {
		class DLLUDM Data {
		public:
			static constexpr auto KEY_ASSET_TYPE = "assetType";
			static constexpr auto KEY_ASSET_VERSION = "assetVersion";
			static constexpr auto KEY_ASSET_DATA = "assetData";
			static std::optional<FormatType> GetFormatType(const std::string &fileName, std::string &outErr);
			static std::optional<FormatType> GetFormatType(std::unique_ptr<IFile> &&f, std::string &outErr);
			static std::optional<FormatType> GetFormatType(const ::VFilePtr &f, std::string &outErr);
			static std::shared_ptr<Data> Load(const std::string &fileName);
			static std::shared_ptr<Data> Load(std::unique_ptr<IFile> &&f);
			static std::shared_ptr<Data> Load(const ::VFilePtr &f);
			static std::shared_ptr<Data> Open(const std::string &fileName);
			static std::shared_ptr<Data> Open(std::unique_ptr<IFile> &&f);
			static std::shared_ptr<Data> Open(const ::VFilePtr &f);
			static std::shared_ptr<Data> Create(const std::string &assetType, Version assetVersion);
			static std::shared_ptr<Data> Create();
			static bool DebugTest();

			PProperty LoadProperty(const std::string_view &path) const;
			void ResolveReferences();

			bool Save(const std::string &fileName) const;
			bool Save(IFile &f) const;
			bool Save(const ::VFilePtr &f);
			bool SaveAscii(const std::string &fileName, AsciiSaveFlags flags = AsciiSaveFlags::Default) const;
			bool SaveAscii(IFile &f, AsciiSaveFlags flags = AsciiSaveFlags::Default) const;
			bool SaveAscii(const ::VFilePtr &f, AsciiSaveFlags flags = AsciiSaveFlags::Default) const;
			Element &GetRootElement() { return *static_cast<Element *>(m_rootProperty->value); }
			const Element &GetRootElement() const { return const_cast<Data *>(this)->GetRootElement(); }
			AssetData GetAssetData() const;

			bool operator==(const Data &other) const;
			bool operator!=(const Data &other) const { return !operator==(other); }

			LinkedPropertyWrapper operator[](const std::string &key) const;
			Element *operator->();
			const Element *operator->() const;
			Element &operator*();
			const Element &operator*() const;

			std::string GetAssetType() const;
			Version GetAssetVersion() const;
			void SetAssetType(const std::string &assetType);
			void SetAssetVersion(Version version);

			void ToAscii(std::stringstream &ss, AsciiSaveFlags flags = AsciiSaveFlags::Default) const;

			const Header &GetHeader() const { return m_header; }

			static std::string ReadKey(IFile &f);
			static void WriteKey(IFile &f, const std::string &key);
		private:
			friend AsciiReader;
			friend ArrayLz4;
			bool ValidateHeaderProperties();
			static void SkipProperty(IFile &f, Type type);
			PProperty LoadProperty(Type type, const std::string_view &path) const;
			static PProperty ReadProperty(IFile &f);
			static void WriteProperty(IFile &f, const Property &o);
			Data() = default;
			Header m_header;
			std::unique_ptr<IFile> m_file = nullptr;
			PProperty m_rootProperty = nullptr;
		};
	}
}
