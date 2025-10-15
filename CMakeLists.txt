include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

option(UTIL_UDM_STATIC "Build as static library?" ON)

if(${UTIL_UDM_STATIC})
	set(LIB_TYPE STATIC)
else()
	set(LIB_TYPE SHARED)
endif()

set(PROJ_NAME util_udm)
pr_add_library(${PROJ_NAME} ${LIB_TYPE})

pr_add_dependency(${PROJ_NAME} vfilesystem TARGET PUBLIC)
pr_add_dependency(${PROJ_NAME} lz4 TARGET)

pr_init_module(${PROJ_NAME})
pr_add_headers(${PROJ_NAME} "include/")

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
