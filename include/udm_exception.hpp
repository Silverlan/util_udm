/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_EXCEPTION_HPP__
#define __UDM_EXCEPTION_HPP__

#include <exception>
#include <string>

namespace udm
{
	struct Exception
		: public std::exception
	{
		Exception(const std::string &msg)
			: std::exception{msg.c_str()},m_msg{msg}
		{}
	private:
		std::string m_msg;
	};

	struct InvalidUsageError : public Exception {using Exception::Exception;};
	struct CompressionError : public Exception {using Exception::Exception;};
	struct FileError : public Exception {using Exception::Exception;};
	struct InvalidFormatError : public Exception {using Exception::Exception;};
	struct PropertyLoadError : public Exception {using Exception::Exception;};
	struct OutOfBoundsError : public Exception {using Exception::Exception;};
	struct ImplementationError : public Exception {using Exception::Exception;};
	struct LogicError : public Exception {using Exception::Exception;};
	struct ComparisonError : public Exception {using Exception::Exception;};
};

#endif
