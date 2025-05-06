set(TARGET MxDBANdsSqlite)

set(SOURCE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/nds_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nds_extensions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nds_extensions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nds_sqlite3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nds_sqlite3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nds_sqlite3ext.h
    ${CMAKE_CURRENT_SOURCE_DIR}/okapi_bm25.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/okapi_bm25.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4hc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4hc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/packerNDSC.c
    ${CMAKE_CURRENT_SOURCE_DIR}/packerNDSC.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rijndael.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rijndael.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.h)

add_library(${TARGET} ${LIBRARY_BUILD_TYPE} ${SOURCE_LIST})

if (RUNNING_STANDALONE)
    # target_compile_options(${TARGET} PRIVATE -O0)
    # target_compile_options(${TARGET} PRIVATE -g3)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_USE_URI)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_COLUMN_METADATA)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_DBSTAT_VTAB)
    target_compile_definitions(${TARGET} PRIVATE NDS_ENABLE_ZLIB)
    target_compile_definitions(${TARGET} PRIVATE NDS_ENABLE_LZ4)
    target_compile_definitions(${TARGET} PRIVATE NDS_ENABLE_NDSC)
    target_compile_definitions(${TARGET} PRIVATE NDS_ENABLE_AES)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_THREADSAFE=1)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_CORE=1)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_FTS3=1)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_FTS3_PARENTHESIS=1)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_FTS4=1)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_FTS5=1)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_RTREE=1)
    target_compile_definitions(${TARGET} PRIVATE NDS_ENABLE_COMPRESS=1)
    target_compile_definitions(${TARGET} PRIVATE NDS_ENABLE_ZSTD)
    target_compile_definitions(${TARGET} PRIVATE NDS_ENABLE_ZSTD_DICT)
    target_compile_definitions(${TARGET} PRIVATE SQLITE_ENABLE_FTS3_TOKENIZER)

    target_compile_options(${TARGET} PRIVATE ${COMPILE_OPTIONS_LIST})
    target_compile_options(${TARGET} PRIVATE -Wno-unused-variable)
    target_compile_options(${TARGET} PRIVATE -Wno-unused-function)
    target_compile_options(${TARGET} PRIVATE -Wno-unneeded-internal-declaration)
    target_compile_options(${TARGET} PRIVATE -Wno-unused-but-set-variable)
    target_compile_options(${TARGET} PRIVATE -Wno-missing-field-initializers)
    target_compile_options(${TARGET} PRIVATE -Wno-macro-redefined)
    target_compile_options(${TARGET} PRIVATE -Wno-sign-compare)
    target_compile_options(${TARGET} PRIVATE -Wno-pointer-sign)
    target_compile_options(${TARGET} PRIVATE -Wno-null-pointer-subtraction)
    target_compile_options(${TARGET} PRIVATE -Wno-incompatible-pointer-types)
    target_compile_options(${TARGET} PRIVATE -Wno-incompatible-pointer-types-discards-qualifiers)
    target_compile_options(${TARGET} PRIVATE -fvisibility=hidden)
else ()
    set_target_properties(${TARGET} PROPERTIES COMPILE_DEFINITIONS "${GLOBAL_COMPILE_DEFINITIONS}")
    set_target_properties(${TARGET} PROPERTIES COMPILE_OPTIONS "${GLOBAL_C_CXX_COMPILE_OPTIONS}")
    add_cxx_compile_options(${TARGET} GLOBAL_CXX_ONLY_WARNING_OPTIONS)
    add_c_compile_options(${TARGET} GLOBAL_C_ONLY_WARNING_OPTIONS)
    set_target_properties(${TARGET} PROPERTIES INCLUDE_DIRECTORIES "${GLOBAL_INCLUDE_DIRECTORIES}")
    set_target_properties(${TARGET} PROPERTIES LINK_OPTIONS "${GLOBAL_LINK_OPTIONS}")
    set_target_properties(${TARGET} PROPERTIES LINK_DIRECTORIES "${GLOBAL_LINK_DIRECTORIES}")
    target_compile_options(${TARGET} PRIVATE -fvisibility=hidden)
    target_compile_options(${TARGET} PRIVATE -w)
    target_compile_options(${TARGET} PRIVATE -Wno-overloaded-virtual)
    target_compile_options(${TARGET} PRIVATE -Wno-incompatible-pointer-types)

    target_compile_options(${TARGET} PRIVATE "-O2")

    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "SQLITE_USE_URI")
    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "SQLITE_ENABLE_COLUMN_METADATA")
    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "SQLITE_ENABLE_DBSTAT_VTAB")
    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "NDS_ENABLE_ZLIB")
    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "NDS_ENABLE_LZ4")
    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "NDS_ENABLE_NDSC")
    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "NDS_ENABLE_AES")
    add_property_to_target(${TARGET} COMPILE_DEFINITIONS "SQLITE_THREADSAFE=1")

    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Wno-reorder")
    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Wno-class-memaccess")
    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Werror=conversion-null")
    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Werror=delete-incomplete")
    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Werror=mismatched-tags")
    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Wno-deprecated-copy")
    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Wno-overloaded-virtual")
    remove_property_from_target(${TARGET} COMPILE_OPTIONS "-Werror=deprecated-declarations")
endif ()

target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${TARGET} PRIVATE libzstd_static MXThirdPartyZlib)
