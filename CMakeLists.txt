cmake_minimum_required(VERSION 3.20)
project(Lactococcus_Lactis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    # Force dynamic CRT to avoid missing runtime symbols on some toolchains
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
endif()

# Ensure nlohmann/json single-header is available under src/third_party/nlohmann/json.hpp
set(NLOHMANN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/nlohmann)
set(NLOHMANN_HEADER ${NLOHMANN_DIR}/json.hpp)
if(NOT EXISTS ${NLOHMANN_HEADER})
    message(FATAL_ERROR "nlohmann/json.hpp not found. Please run: mkdir -p src/third_party/nlohmann && curl -L -o src/third_party/nlohmann/json.hpp https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp")
endif()

# Common static library for shared utilities and protocol implementations
file(GLOB_RECURSE COMMON_HTTP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/common/http_util/*.cpp")
file(GLOB_RECURSE COMMON_PROTOCOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/common/protocol/*.cpp")
file(GLOB_RECURSE SERVER_HTTP_UTIL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/server/http_util/*.cpp")

add_library(common_core STATIC
    ${COMMON_HTTP_SOURCES}
    ${COMMON_PROTOCOL_SOURCES}
    ${SERVER_HTTP_UTIL_SOURCES}
)
target_include_directories(common_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(common_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party)

# Server core library (everything under src/server except http_util which lives in common_core)
file(GLOB_RECURSE SERVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cpp")
foreach(hf IN LISTS SERVER_HTTP_UTIL_SOURCES)
    list(REMOVE_ITEM SERVER_SOURCES ${hf})
endforeach()

add_library(server_core STATIC ${SERVER_SOURCES})
target_include_directories(server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party)
target_link_libraries(server_core PUBLIC common_core)

if (WIN32)
    target_link_libraries(server_core PRIVATE Ws2_32)
endif()

# Optional OpenSSL: if found, link and enable TLS support
find_package(OpenSSL)
if(OpenSSL_FOUND)
    message(STATUS "Found OpenSSL: enabling TLS support")
    target_link_libraries(server_core PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(server_core PRIVATE HAVE_OPENSSL=1)
else()
    message(STATUS "OpenSSL not found: building without TLS support")
endif()

# Server executable
add_executable(server_app src/server/Main.cpp)
target_link_libraries(server_app PRIVATE server_core)

# Tests (if present)
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_src IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE server_core)
endforeach()

message(STATUS "CMake configuration completed")
