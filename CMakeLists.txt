cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(nds_handle_manager LANGUAGES C CXX)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_ROOT_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(LIBRARY_BUILD_TYPE STATIC)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
endif ()

option(TESTING_ENABLED "Enable test" ON)
option(RUNNING_STANDALONE "Running net in standalone mode." ON)

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif ()

if (TESTING_ENABLED)
    add_definitions(-DTESTING_ENABLED)
endif ()

if (RUNNING_STANDALONE)
    add_definitions(-DRUNNING_STANDALONE)
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif ()

# set(CMAKE_CXX_FLAGS_DEBUG "-O0")
# set(CMAKE_CXX_FLAGS_RELEASE "-O2")
# set(CMAKE_C_FLAGS_DEBUG "-O0")
# set(CMAKE_C_FLAGS_RELEASE "-O2")

set(COMPILE_OPTIONS_LIST
        -Wall
        -Wextra
        -Woverloaded-virtual
        -Wpointer-arith
        -Wwrite-strings
        -Wformat
        -Wformat=2
        -Wformat-extra-args
        -Wformat-zero-length
        -Wno-format-security
        -Werror
        -Wno-unused-parameter
        -Wno-format-nonliteral
        -march=native
        -fno-omit-frame-pointer)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    list(APPEND COMPILE_OPTIONS_LIST -ggdb3)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMPILE_OPTIONS_LIST
        -g3
        -fno-limit-debug-info
        -Wformat-insufficient-args
        -Wformat-invalid-specifier)
endif()

# if (CMAKE_BUILD_TYPE STREQUAL "Debug")
#     add_compile_options(-O0)
# else()
#     add_compile_options(-O2)
# endif ()
list(APPEND COMPILE_OPTIONS_LIST -O0 -ggdb3)

# set sanitizer options
if(CMAKE_SANITIZER_TYPE STREQUAL "-fsanitize=address")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
elseif(CMAKE_SANITIZER_TYPE STREQUAL "-fsanitize=thread")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
elseif(CMAKE_SANITIZER_TYPE STREQUAL "-fsanitize=undefined")
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

if(NOT CMAKE_GENERATOR)
    set(CMAKE_GENERATOR "Unix Makefiles" CACHE INTERNAL "")
endif()

if (TESTING_ENABLED)
    enable_testing()
endif ()

# print messages
message("-- TESTING_ENABLED: ${TESTING_ENABLED}")
message("-- RUNNING_STANDALONE: ${RUNNING_STANDALONE}")
message("-- CCACHE_FOUND: ${CCACHE_FOUND}")
message("-- CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("-- CMAKE_SANITIZER_TYPE: ${CMAKE_SANITIZER_TYPE}")
message("-- CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message("-- COMPILE_OPTIONS_LIST: ${COMPILE_OPTIONS_LIST}")
message("-- CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message("-- CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/nds_sqlite)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dbs_handle_manager)

# add_executable(handle_manager ${CMAKE_CURRENT_SOURCE_DIR}/example.cpp)
# target_link_libraries(handle_manager PRIVATE MxSqliteHandleManager)

add_executable(generate_db ${CMAKE_CURRENT_SOURCE_DIR}/generate_db.cpp)
target_link_libraries(generate_db PRIVATE MxDBANdsSqlite)

add_executable(read_db ${CMAKE_CURRENT_SOURCE_DIR}/read_db.cpp)
target_link_libraries(read_db PRIVATE MxDBANdsSqlite)

add_executable(read_db2 ${CMAKE_CURRENT_SOURCE_DIR}/read_db2.cpp)
target_link_libraries(read_db2 PRIVATE MxDBANdsSqlite sqlite_orm)

add_executable(orm_test ${CMAKE_CURRENT_SOURCE_DIR}/orm_test.cpp)
target_link_libraries(orm_test PRIVATE MxDBANdsSqlite sqlite_orm)

add_executable(handle_manager_test
    ${CMAKE_CURRENT_SOURCE_DIR}/handle_manager_sqlite.h
    ${CMAKE_CURRENT_SOURCE_DIR}/handle_manager_sqlite.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/user.h
    ${CMAKE_CURRENT_SOURCE_DIR}/user.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/order.h
    ${CMAKE_CURRENT_SOURCE_DIR}/order.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/handle_manager_example.cpp)
target_link_libraries(handle_manager_test PRIVATE MxDBANdsSqlite sqlite_orm)
