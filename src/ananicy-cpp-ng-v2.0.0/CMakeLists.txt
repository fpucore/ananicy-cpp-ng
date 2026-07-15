cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Version)

project(ananicy-cpp-ng
        VERSION ${VERSION_FOR_CMAKE}
        LANGUAGES CXX)

include(GNUInstallDirs)
include(CompilerChecks)
include(StandardProjectSettings)
include(EnableCcache)
include(Linker)
include(PGO)
include(StaticAnalyzers)
include(Sanitizers)
include(CPM)

# Link this 'library' to set the c++ standard / compile-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)

list(APPEND SOURCES
        src/main.cpp
        src/utils.cpp
        src/rules.cpp
        src/worker.cpp
        src/config.cpp
        src/utility/argument_parsing/argument_parser.cpp
        src/utility/argument_parsing/argument.cpp)

list(APPEND HEADERS
        include/config.hpp
        include/core/priority.hpp
        include/core/process.hpp
        include/core/rules.hpp
        include/utility/utils.hpp
        include/core/worker.hpp
        include/core/cgroups.hpp
        include/version.hpp
        include/service.hpp
        include/utility/backtrace.hpp
        include/utility/argument_parser.hpp
        include/utility/debug.hpp
        include/utility/process_info.hpp)

list(APPEND LINUX_SOURCES
        src/platform/linux/priority.cpp
        src/platform/linux/process.cpp
        src/platform/linux/syscalls.h
        src/platform/linux/backtrace.cpp
        src/platform/linux/cgroups.cpp
        src/platform/linux/debug.cpp
        src/platform/linux/singleton_process.cpp
        src/platform/linux/process_info.cpp)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(STATUS "Linux system detected, adding specific sources files")
    list(APPEND SOURCES ${LINUX_SOURCES})
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    message(FATAL_ERROR "Mac OS (Darwin) system detected. It is not supported.")
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} system detected. It is not supported.")
endif()

if(ENABLE_SYSTEMD)
    list(APPEND SOURCES
            src/platform/systemd/service.cpp)
else()
    list(APPEND SOURCES
            src/platform/generic/service.cpp)
endif()

add_executable(ananicy-cpp-ng ${HEADERS} ${SOURCES})
target_sources(ananicy-cpp-ng PRIVATE src include)

if(STATIC)
    set_target_properties(ananicy-cpp-ng PROPERTIES LINK_SEARCH_START_STATIC ON)
    set_target_properties(ananicy-cpp-ng PROPERTIES LINK_SEARCH_END_STATIC ON)
    set_target_properties(ananicy-cpp-ng PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_options(ananicy-cpp-ng PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Linker config
configure_linker(project_options)

# Sanitizer options (if supported by compiler)
enable_sanitizers(project_options)

# Threading support
message(STATUS "Configuring threads")
set(CMAKE_MESSAGE_CONTEXT "threads")
find_package(Threads REQUIRED)
target_link_libraries(
ananicy-cpp-ng PRIVATE Threads::Threads)
set(CMAKE_MESSAGE_CONTEXT "")

# nlohmann_json
message(STATUS "Configuring nlohmann_json")
set(CMAKE_MESSAGE_CONTEXT "nlohmann_json")
if (USE_EXTERNAL_JSON)
    find_package(nlohmann_json 3.9 REQUIRED)
else()
    CPMAddPackage(
        NAME json
        GITHUB_REPOSITORY ArthurSonzogni/nlohmann_json_cmake_fetchcontent
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        GIT_TAG v3.11.2
        EXCLUDE_FROM_ALL YES)
endif()
target_link_libraries(ananicy-cpp-ng PRIVATE "nlohmann_json::nlohmann_json")
set(CMAKE_MESSAGE_CONTEXT "")

# fmtlib
set(CMAKE_MESSAGE_CONTEXT "fmtlib")
message(STATUS "Configuring fmtlib")
if(USE_EXTERNAL_FMTLIB)
    message("Using external fmtlib")
    # Stand-alone build
    find_package(fmt 8.0 REQUIRED)
else()
    CPMAddPackage(
        NAME fmt
        GITHUB_REPOSITORY fmtlib/fmt
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        GIT_TAG 10.1.0
        EXCLUDE_FROM_ALL YES
    )
endif()
set(CMAKE_MESSAGE_CONTEXT "")

# std::format
set(STL_FORMAT_USE_EXTERNAL_FMTLIB ON CACHE BOOL "" FORCE)
#if (NOT TARGET stl_polyfill::format)
#    add_subdirectory(external/std-format)
#endif()
#target_link_libraries(ananicy-cpp-ng PRIVATE stl_polyfill::format)
#set(CMAKE_MESSAGE_CONTEXT "")

# spdlog
set(CMAKE_MESSAGE_CONTEXT "spdlog")
message(STATUS "Configuring spdlog")
set(SPDLOG_FMT_EXTERNAL ON CACHE INTERNAL "Make spdlog use external fmtlib" FORCE)
if(USE_EXTERNAL_SPDLOG)
    message("Using external spdlog")
    # Stand-alone build
    find_package(spdlog 1.9 REQUIRED)
else()
    CPMAddPackage(
        NAME spdlog
        GITHUB_REPOSITORY gabime/spdlog
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        GIT_TAG v1.12.0
        EXCLUDE_FROM_ALL YES)
endif()
target_link_libraries(ananicy-cpp-ng PRIVATE project_options spdlog::spdlog fmt::fmt rt)
set(CMAKE_MESSAGE_CONTEXT "")


if(ENABLE_SYSTEMD)
    message(STATUS "Configuring systemd")
    set(CMAKE_MESSAGE_CONTEXT "systemd")
    add_compile_definitions(ENABLE_SYSTEMD=1)
    target_link_libraries(ananicy-cpp-ng PRIVATE "systemd")
    set(CMAKE_MESSAGE_CONTEXT "")
endif()

if(USE_BPF_PROC_IMPL)
    add_subdirectory(libananicycppng_bpf)
    add_dependencies(ananicy-cpp-ng ananicy_cpp_ng_bpf_c::ananicy_cpp_ng_bpf_c)
    target_link_libraries(ananicy-cpp-ng PRIVATE ananicy_cpp_ng_bpf_cpp::ananicy_cpp_ng_bpf_cpp ananicy_cpp_ng_bpf_c::ananicy_cpp_ng_bpf_c)
else()
    add_subdirectory(libananicycppng_netlink)
    add_dependencies(ananicy-cpp-ng ananicy_cpp_ng_netlink_c::ananicy_cpp_ng_netlink_c)
    target_link_libraries(ananicy-cpp-ng PRIVATE ananicy_cpp_ng_netlink_cpp::ananicy_cpp_ng_netlink_cpp ananicy_cpp_ng_netlink_c::ananicy_cpp_ng_netlink_c)
endif()

if(ENABLE_ANANICY_TESTS)
    add_subdirectory(src/tests)
endif()

if(ENABLE_ANANICY_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

target_include_directories(ananicy-cpp-ng PRIVATE "${CMAKE_SOURCE_DIR}/include")

install(TARGETS ananicy-cpp-ng
        RUNTIME DESTINATION bin COMPONENT Runtime)

if(ENABLE_SYSTEMD)
    configure_file(ananicy-cpp-ng.service ananicy-cpp-ng.service @ONLY)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ananicy-cpp-ng.service
            DESTINATION lib/systemd/system/
            COMPONENT Runtime)
endif()
