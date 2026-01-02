include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

option(UTIL_UDM_STATIC "Build as static library?" ON)

if(UTIL_UDM_STATIC)
	set(LIB_TYPE STATIC)
else()
	set(LIB_TYPE SHARED)
endif()

set(PROJ_NAME util_udm)
pr_add_library(${PROJ_NAME} ${LIB_TYPE})
pr_add_export_macro(${PROJ_NAME} DLLUDM ${LIB_TYPE})

pr_add_dependency(${PROJ_NAME} vfilesystem TARGET PUBLIC)
pr_add_dependency(${PROJ_NAME} lz4 TARGET)

# MSVC currently has a bug with C++20 modules that causes unresolved
# external symbol errors when building UDM with separate module partitions.
# As a temporary workaround, we use merged.cppm, which merges all partitions into one.
# For other compilers we use the separate partitions as intended. Once the MSVC bug has been fixed,
# merged.cppm, as well as the code below, can be deleted.
if(MSVC)
	file(GLOB_RECURSE MODULE_EXCLUSION_LIST "src/*.cppm")
	list(FILTER MODULE_EXCLUSION_LIST EXCLUDE REGEX "[/\\]merged\\.cppm$")
	target_compile_definitions(${PROJ_NAME} PRIVATE "UDM_SINGLE_MODULE_INTERFACE=1")
else()
	file(GLOB_RECURSE MODULE_EXCLUSION_LIST "src/merged.cppm")
endif()

pr_init_module(${PROJ_NAME} EXCLUDE ${MODULE_EXCLUSION_LIST})

if(NOT ${UTIL_UDM_STATIC})
	pr_add_compile_definitions(
		${PROJ_NAME}
			-DUDM_SHARED
			-DUDM_EXPORT
			-DSHUTIL_STATIC
			-DMUTIL_STATIC
			-DVFILESYSTEM_STATIC
	)
endif()

if(MSVC)
	target_compile_options(${PROJ_NAME} PRIVATE "/bigobj")
else()
	target_link_options(${PROJ_NAME} PRIVATE "LINKER:--strip-all")
endif()

pr_finalize(${PROJ_NAME})
