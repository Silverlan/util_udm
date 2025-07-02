// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __UDM_DEFINITIONS_HPP__
#define __UDM_DEFINITIONS_HPP__

#ifndef UDM_SHARED
#define DLLUDM
#elif UDM_EXPORT
#ifdef __linux__
#define DLLUDM __attribute__((visibility("default")))
#else
#define DLLUDM __declspec(dllexport)
#endif
#else
#ifdef __linux__
#define DLLUDM
#else
#define DLLUDM __declspec(dllimport)
#endif
#endif

namespace udm {
	static constexpr auto ENABLE_COMPARISON_EXCEPTION = false;
};
#define UDM_ASSERT_COMPARISON(res)                                                                                                                                                                                                                                                               \
	if constexpr(ENABLE_COMPARISON_EXCEPTION) {                                                                                                                                                                                                                                                  \
		if(!res)                                                                                                                                                                                                                                                                                 \
			throw ComparisonError {std::string {"Comparison failure "} + " in " + __FILE__ + ':' + std::to_string(__LINE__) + ':' + __func__};                                                                                                                                                   \
	}

#endif
