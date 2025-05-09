set(TARGET MxDBSHandleManager)

set(SOURCE_LISTS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/dbs/dbs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/dbs/service/a_service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inner_defines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/path_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/path_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/storage_container.h
    ${CMAKE_CURRENT_SOURCE_DIR}/storage_container.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/a_storage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/a_storage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/service/a_service.cpp
)

add_library(${TARGET} ${LIBRARY_BUILD_TYPE} ${SOURCE_LISTS})

#=========================================================================================================

if (RUNNING_STANDALONE)
    target_compile_options(${TARGET} PRIVATE ${COMPILE_OPTIONS_LIST})
else ()
    # do nothing
endif ()

target_link_libraries(${TARGET} PRIVATE MxDBANdsSqlite sqlite_orm::sqlite_orm)
target_include_directories(${TARGET}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 禁用 sqlite_orm 库中未使用类型别名的警告
target_compile_options(${TARGET} PRIVATE -Wno-unused-local-typedef)

#=========================================================================================================

if (RUNNING_STANDALONE AND TESTING_ENABLED)
    add_executable(test_a_service ${CMAKE_CURRENT_SOURCE_DIR}/service/a_service_test.cpp)
    target_compile_options(test_a_service PRIVATE ${COMPILE_OPTIONS_LIST})
    target_link_libraries(test_a_service PRIVATE ${TARGET} MxDBSHandleManager GTest::gtest GTest::gtest_main)
    add_test(NAME test_a_service COMMAND test_a_service)
    set_tests_properties(test_a_service PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()