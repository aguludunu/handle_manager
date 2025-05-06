# compile common dependency libs
message(STATUS "compile common.centauri.thirdparty: common dependency libs ...")

if (TESTING_ENABLED)
    # googletest
	set(gtest_build_tests OFF CACHE BOOL "" FORCE)
	set(gtest_build_samples OFF CACHE BOOL "" FORCE)
	set(gmock_build_tests OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/googletest)

	# benchmark
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_USE_BUNDLED_GTEST OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmark)
    #if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    #    target_compile_options(benchmark PRIVATE -Wno-maybe-uninitialized)
    #endif()
endif ()

# abseil-cpp
set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/abseil-cpp)

# MXThirdPartyZlib
set(TARGET_NAME_Zlib MXThirdPartyZlib)

set(SOURCE_LIST_Zlib
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/adler32.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/crc32.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/infback.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/inflate.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/trees.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/zutil.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/compress.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/deflate.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/inffast.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/inftrees.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/uncompr.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/gzclose.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/gzlib.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/gzread.c
${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib/gzwrite.c)
add_library(${TARGET_NAME_Zlib} ${LIBRARY_BUILD_TYPE} ${SOURCE_LIST_Zlib})

if (RUNNING_STANDALONE)
    target_compile_options(${TARGET_NAME_Zlib} PRIVATE ${COMPILE_OPTIONS_LIST})
else ()
if(ANDROID)
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES COMPILE_DEFINITIONS "${GLOBAL_COMPILE_DEFINITIONS}")
    add_properties_to_target(${TARGET_NAME_Zlib} COMPILE_OPTIONS "${GLOBAL_C_CXX_COMPILE_OPTIONS}")
    add_cxx_compile_options(${TARGET_NAME_Zlib} GLOBAL_CXX_ONLY_WARNING_OPTIONS)
    add_c_compile_options(${TARGET_NAME_Zlib} GLOBAL_C_ONLY_WARNING_OPTIONS)
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES INCLUDE_DIRECTORIES "${GLOBAL_INCLUDE_DIRECTORIES}")
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES LINK_OPTIONS "${GLOBAL_LINK_OPTIONS}")
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES LINK_DIRECTORIES "${GLOBAL_LINK_DIRECTORIES}")
    target_compile_options(${TARGET_NAME_Zlib} PRIVATE -fvisibility=hidden)
else()
    target_compile_options(${TARGET_NAME_Zlib} PRIVATE -fvisibility=hidden)
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES COMPILE_DEFINITIONS "${GLOBAL_COMPILE_DEFINITIONS}")
    add_properties_to_target(${TARGET_NAME_Zlib} COMPILE_OPTIONS "${GLOBAL_C_CXX_COMPILE_OPTIONS}")
    add_cxx_compile_options(${TARGET_NAME_Zlib} GLOBAL_CXX_ONLY_WARNING_OPTIONS)
    add_c_compile_options(${TARGET_NAME_Zlib} GLOBAL_C_ONLY_WARNING_OPTIONS)
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES INCLUDE_DIRECTORIES "${GLOBAL_INCLUDE_DIRECTORIES}")
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES LINK_OPTIONS "${GLOBAL_LINK_OPTIONS}")
    set_target_properties(${TARGET_NAME_Zlib} PROPERTIES LINK_DIRECTORIES "${GLOBAL_LINK_DIRECTORIES}")
endif()
endif()
target_compile_options(${TARGET_NAME_Zlib} PRIVATE -w)

target_include_directories(${TARGET_NAME_Zlib} SYSTEM
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compress/include/zlib
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compress/zlib)

# zstd
set(ZSTD_DISABLE_INSTALL ON)
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_SHARED OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zstd-1.5.4/build/cmake)
set_target_properties(libzstd_static PROPERTIES OUTPUT_NAME zstd_1.5.4)

# sqlite_orm
set(SQLITE_ORM_ENABLE_CXX_17 ON)
set(BUILD_TESTING OFF)
set(BUILD_EXAMPLES OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sqlite_orm)
