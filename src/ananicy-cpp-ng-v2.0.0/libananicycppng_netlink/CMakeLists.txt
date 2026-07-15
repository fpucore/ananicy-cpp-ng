cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

project(ananicy_cpp_ng_netlink
        LANGUAGES CXX C)

option(NETLINK_BUILD_SAMPLES "Build ananicy_cpp_ng_netlink samples" OFF)

add_library(ananicy_cpp_ng_netlink_c STATIC
  src/netlink_program_utils.c
)
target_include_directories(ananicy_cpp_ng_netlink_c PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(ananicy_cpp_ng_netlink_c::ananicy_cpp_ng_netlink_c ALIAS ananicy_cpp_ng_netlink_c)

add_library(ananicy_cpp_ng_netlink_cpp INTERFACE)
target_include_directories(ananicy_cpp_ng_netlink_cpp INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include_cpp>)
add_dependencies(ananicy_cpp_ng_netlink_cpp ananicy_cpp_ng_netlink_c)
add_library(ananicy_cpp_ng_netlink_cpp::ananicy_cpp_ng_netlink_cpp ALIAS ananicy_cpp_ng_netlink_cpp)


if(NETLINK_BUILD_SAMPLES)
  add_executable(netlink_proc_cpp main.cpp)
  target_compile_features(netlink_proc_cpp PUBLIC cxx_std_20)
  target_compile_definitions(netlink_proc_cpp PUBLIC -DSPDLOG_FMT_EXTERNAL -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
  target_link_libraries(netlink_proc_cpp PRIVATE spdlog fmt ananicy_cpp_ng_netlink_cpp::ananicy_cpp_ng_netlink_cpp ananicy_cpp_ng_netlink_c::ananicy_cpp_ng_netlink_c)
endif()
