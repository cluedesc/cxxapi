cmake_minimum_required(VERSION 3.16)

project("cxxapi" VERSION 1.1.2 LANGUAGES CXX)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(CXXAPI_USE_NLOHMANN_JSON "Enable nlohmann/json support" ON)
option(CXXAPI_USE_GLAZE_JSON "Enable Glaze JSON support" OFF)

option(CXXAPI_USE_REDIS_IMPL "Enable Redis CXXAPI implementation support" ON)
option(CXXAPI_USE_LOGGING_IMPL "Enable Logging CXXAPI implementation support" ON)

option(CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL "Enable builtin middlewares support" ON)

message(STATUS "CXXAPI configuration summary:")
message(STATUS "  CXXAPI_USE_NLOHMANN_JSON = ${CXXAPI_USE_NLOHMANN_JSON}")
message(STATUS "  CXXAPI_USE_GLAZE_JSON    = ${CXXAPI_USE_GLAZE_JSON}")
message(STATUS "  CXXAPI_USE_REDIS_IMPL    = ${CXXAPI_USE_REDIS_IMPL}")
message(STATUS "  CXXAPI_USE_LOGGING_IMPL  = ${CXXAPI_USE_LOGGING_IMPL}")
message(STATUS "  CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL = ${CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL}")

if(NOT (CXXAPI_USE_NLOHMANN_JSON OR CXXAPI_USE_GLAZE_JSON))
    message(FATAL_ERROR "  At least one of CXXAPI_USE_NLOHMANN_JSON or CXXAPI_USE_GLAZE_JSON must be ON")
endif()

if (CXXAPI_USE_GLAZE_JSON)
    set(CMAKE_CXX_STANDARD 23)

    message(STATUS "  JSON: Glaze (C++23 required)")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libstdc++")
    endif()

    include(CheckCXXSourceCompiles)

    check_cxx_source_compiles("
        #include <expected>
        
        int main() {
            std::expected<int, int> e(1);

            return 0;
        }
    " CXXAPI_HAS_STD_EXPECTED)

    if(NOT CXXAPI_HAS_STD_EXPECTED)
        message(FATAL_ERROR "  Your compiler/STL does not support std::expected (C++23 feature required by glaze). Please use a newer compiler or disable CXXAPI_USE_GLAZE_JSON.")
    endif()
elseif (CXXAPI_USE_NLOHMANN_JSON)
    set(CMAKE_CXX_STANDARD 20)

    message(STATUS "  JSON used: nlohmann/json")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_definitions(__MSVC_COMPILER__)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_definitions(__GCC_COMPILER__)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions(__CLANG_COMPILER__)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    add_compile_definitions(__INTEL_COMPILER__)
else()
    message(FATAL_ERROR "  Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/*.cxx")

if(NOT CXXAPI_USE_REDIS_IMPL)
    list(FILTER SOURCES EXCLUDE REGEX "redis")
endif()

if(NOT CXXAPI_USE_LOGGING_IMPL)
    list(FILTER SOURCES EXCLUDE REGEX "logging")
endif()

if(NOT CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL)
    list(FILTER SOURCES EXCLUDE REGEX "cors")
endif()

add_library(cxxapi ${SOURCES})
add_library(cxxapi::cxxapi ALIAS cxxapi)

if(CXXAPI_USE_NLOHMANN_JSON)
    target_include_directories(cxxapi PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/shared/thirdparty/json/single_include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(cxxapi PUBLIC CXXAPI_USE_NLOHMANN_JSON)
endif()

if(CXXAPI_USE_GLAZE_JSON)
    target_include_directories(cxxapi PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/shared/thirdparty/glaze/include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(cxxapi PUBLIC CXXAPI_USE_GLAZE_JSON)
endif()

target_include_directories(cxxapi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/shared/thirdparty/fmt/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(cxxapi PUBLIC BOOST_ASIO_HAS_IO_URING=1)

target_compile_definitions(cxxapi PUBLIC FMT_HEADER_ONLY=1)

target_include_directories(cxxapi
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxxapi>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/shared
)

if (CXXAPI_USE_REDIS_IMPL)
    target_compile_definitions(cxxapi PUBLIC CXXAPI_USE_REDIS_IMPL)

    message(STATUS "  Redis: enabled")
endif()

if (CXXAPI_USE_LOGGING_IMPL)
    target_compile_definitions(cxxapi PUBLIC CXXAPI_USE_LOGGING_IMPL)

    message(STATUS "  Logging: enabled")
endif()

if (CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL)
    target_compile_definitions(cxxapi PUBLIC CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL)

    message(STATUS "  Middlewares: enabled")
endif()

find_package(Boost 1.84.0 REQUIRED COMPONENTS system filesystem iostreams)

if(Boost_FOUND)
    target_link_libraries(cxxapi PUBLIC Boost::system Boost::filesystem Boost::iostreams)
else()
    message(FATAL_ERROR "  Boost not found")
endif()

find_package(OpenSSL REQUIRED)

if(OpenSSL_FOUND)
    target_link_libraries(cxxapi PUBLIC OpenSSL::SSL OpenSSL::Crypto)
else()
    message(FATAL_ERROR "  OpenSSL not found")
endif()

find_package(Threads REQUIRED)

if (Threads_FOUND)
    target_link_libraries(cxxapi PUBLIC Threads::Threads)
else()
    message(FATAL_ERROR "  Threads not found")
endif()

target_link_libraries(cxxapi PUBLIC uring)

include(GNUInstallDirs)

set(CXXAPI_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/cxxapi")

install(TARGETS cxxapi
    EXPORT cxxapi-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/"
    DESTINATION ${CXXAPI_INSTALL_INCLUDEDIR}
    FILES_MATCHING 
    PATTERN "*.h" 
    PATTERN "*.hxx" 
    PATTERN "*.hpp"
    PATTERN "redis/*" EXCLUDE
    PATTERN "logging/*" EXCLUDE
)

if(CXXAPI_USE_REDIS_IMPL)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/shared/redis/"
        DESTINATION ${CXXAPI_INSTALL_INCLUDEDIR}/shared/redis
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hxx"
        PATTERN "*.hpp"
    )
endif()

if(CXXAPI_USE_LOGGING_IMPL)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/shared/logging/"
        DESTINATION ${CXXAPI_INSTALL_INCLUDEDIR}/shared/logging
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hxx"
        PATTERN "*.hpp"
    )
endif()

if(CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cxxapi/middlewares/cors"
        DESTINATION ${CXXAPI_INSTALL_INCLUDEDIR}/cxxapi/middlewares/cors
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hxx"
        PATTERN "*.hpp"
    )
endif()

install(EXPORT cxxapi-targets
    FILE cxxapi-targets.cmake
    NAMESPACE cxxapi::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxxapi
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cxxapi-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cxxapi-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxxapi
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cxxapi-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cxxapi-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cxxapi-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxxapi
)

option(CXXAPI_BUILD_TESTS "Build CXXAPI tests" ON)

if(CXXAPI_BUILD_TESTS)
    message(STATUS "  Tests: enabled")
    
    include(FetchContent)

    FetchContent_Declare(
        googletest

        URL https://github.com/google/googletest/archive/refs/heads/main.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # ...
    set(
        TEST_SOURCES 

        tests/main.cxx 
        tests/shared/json_traits/test_json_traits.cxx
        tests/cxxapi/http/test_http.cxx
        tests/cxxapi/http/test_http_ctx.cxx
        tests/cxxapi/http/test_http_multipart.cxx
        tests/cxxapi/route/test_route.cxx
        tests/cxxapi/middleware/test_middleware.cxx
    )

    if(CXXAPI_USE_REDIS_IMPL)
        list(APPEND TEST_SOURCES tests/shared/redis/test_redis.cxx)
    endif()

    if(CXXAPI_USE_LOGGING_IMPL)
        list(APPEND TEST_SOURCES tests/shared/logging/test_logging.cxx)
    endif()
    # ...

    add_executable(cxxapi_tests ${TEST_SOURCES})

    target_link_libraries(cxxapi_tests
        PRIVATE
        cxxapi
        GTest::gtest_main
    )

    include(GoogleTest)

    gtest_discover_tests(cxxapi_tests)
endif()
