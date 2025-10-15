// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "udm_definitions.hpp"
#include <exception>
#include <stdexcept>
#include <string>

export module pragma.udm:exception;

export {
	namespace udm {
		struct DLLUDM Exception : public std::runtime_error {
			Exception(const std::string &msg) : std::runtime_error {msg.c_str()}, m_msg {msg} {}
		private:
			std::string m_msg;
		};

		struct DLLUDM InvalidUsageError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM CompressionError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM FileError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM InvalidFormatError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM PropertyLoadError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM OutOfBoundsError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM ImplementationError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM LogicError : public Exception {
			using Exception::Exception;
		};
		struct DLLUDM ComparisonError : public Exception {
			using Exception::Exception;
		};

		struct DLLUDM AsciiException : public Exception {
			AsciiException(const std::string &msg, uint32_t lineIdx, uint32_t charIdx);
			uint32_t lineIndex = 0;
			uint32_t charIndex = 0;
		};

		struct DLLUDM SyntaxError : public AsciiException {
			using AsciiException::AsciiException;
		};
		struct DLLUDM DataError : public AsciiException {
			using AsciiException::AsciiException;
		};
	};
}
