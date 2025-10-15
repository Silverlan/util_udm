// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

export module pragma.udm:file;

export import pragma.filesystem;

export {
	namespace udm {
		using IFile = ufile::IFile;
		using MemoryFile = ufile::MemoryFile;
		using VectorFile = ufile::VectorFile;
	};
}
