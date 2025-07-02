// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __UDM_EXCEPTION_HPP__
#define __UDM_EXCEPTION_HPP__

#include "udm_definitions.hpp"
#include <exception>
#include <stdexcept>
#include <string>

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
};

#endif
