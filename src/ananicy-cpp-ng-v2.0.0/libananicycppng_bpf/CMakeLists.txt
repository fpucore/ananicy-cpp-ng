cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

project(ananicy_cpp_ng_bpf
        LANGUAGES CXX C)

include(CPM)
include(ExternalProject)

option(BPF_BUILD_SAMPLES "Build ananicy_cpp_ng_bpf samples" OFF)
option(BPF_BUILD_LIBBPF  "Build libbpf" ON)
option(BPF_BUILD_BPFTOOL "Build bpftool" OFF)

if(BPF_BUILD_LIBBPF)
  CPMAddPackage(
      NAME libbpf
      GITHUB_REPOSITORY libbpf/libbpf
      GIT_TAG e3a40329bb05a333fc588e3bf50365a554fda0a6
      DOWNLOAD_ONLY YES
      EXCLUDE_FROM_ALL YES
  )

  if(libbpf_ADDED)
    # libbpf has no CMakeLists, so we create our own target

    # Build vendored libbpf
    #ExternalProject_Add(libbpf
    #  PREFIX libbpf
    #  SOURCE_DIR ${libbpf_SOURCE_DIR}/src
    #  CONFIGURE_COMMAND ""
    #  BUILD_COMMAND make
    #    BUILD_STATIC_ONLY=1
    #    OBJDIR=${CMAKE_CURRENT_BINARY_DIR}/libbpf/libbpf
    #    DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/libbpf
    #    INCLUDEDIR=
    #    LIBDIR=
    #    UAPIDIR=
    #    install
    #  BUILD_IN_SOURCE TRUE
    #  INSTALL_COMMAND ""
    #  STEP_TARGETS build
    #)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libbpf/bpf)

    file(GLOB libbpf_sources ${libbpf_SOURCE_DIR}/src/*.c)
    file(GLOB libbpf_headers ${libbpf_SOURCE_DIR}/src/*.h)

    add_library(bpf STATIC ${libbpf_sources})
    add_library(bpf::bpf ALIAS bpf)

    foreach(libbpf_header ${libbpf_headers})
        add_custom_command(
            TARGET bpf
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy ${libbpf_header} ${CMAKE_CURRENT_BINARY_DIR}/libbpf/bpf
        )
    endforeach()

    set_target_properties(bpf PROPERTIES COMPILE_FLAGS "-g -O2 -Werror -Wall -std=gnu89 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64")

    target_include_directories(bpf SYSTEM PUBLIC $<BUILD_INTERFACE:${libbpf_SOURCE_DIR}>)
    target_include_directories(bpf SYSTEM PUBLIC $<BUILD_INTERFACE:${libbpf_SOURCE_DIR}/src>)
    target_include_directories(bpf SYSTEM PUBLIC $<BUILD_INTERFACE:${libbpf_SOURCE_DIR}/include>)
    target_include_directories(bpf SYSTEM PUBLIC $<BUILD_INTERFACE:${libbpf_SOURCE_DIR}/include/uapi>)
    target_include_directories(bpf SYSTEM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/libbpf>)
  endif()

  set(LIBBPF_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/libbpf")
  set(LIBBPF_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/libbpf.a")
endif()

if(BPF_BUILD_BPFTOOL)
  CPMAddPackage(
      NAME bpftool
      GITHUB_REPOSITORY libbpf/bpftool
      GIT_PROGRESS TRUE
      GIT_SHALLOW TRUE
      GIT_TAG 8e721a476f58cb2ce16035cbaeca0352174aaa9e
      DOWNLOAD_ONLY YES
      EXCLUDE_FROM_ALL YES
  )

  if(bpftool_ADDED)
    # bpftool has no CMakeLists, so we create our own target

    # Build vendored bpftool
    ExternalProject_Add(bpftool
      PREFIX bpftool
      SOURCE_DIR ${bpftool_SOURCE_DIR}/src
      CONFIGURE_COMMAND ""
      BUILD_COMMAND make
        OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bpftool/
      BUILD_IN_SOURCE TRUE
      INSTALL_COMMAND ""
      STEP_TARGETS build
    )
  endif()
  set(BPFOBJECT_BPFTOOL_EXE "${CMAKE_CURRENT_BINARY_DIR}/bpftool/bpftool")
endif()

# Get target arch
execute_process(COMMAND uname -m
  COMMAND sed -e "s/x86_64/x86/" -e "s/aarch64/arm64/" -e "s/ppc64le/powerpc/" -e "s/mips.*/mips/" -e "s/riscv64/riscv/" -e "s/loongarch.*/loongarch/"
  OUTPUT_VARIABLE ARCH_output
  ERROR_VARIABLE ARCH_error
  RESULT_VARIABLE ARCH_result
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(${ARCH_result} EQUAL 0)
  set(PROJECT_ARCH ${ARCH_output})
else()
  message(FATAL_ERROR "Failed to determine target architecture: ${ARCH_error}")
endif()

# Set BpfObject input parameters -- note this is usually not necessary unless
# you're in a highly vendored environment (like libbpf-bootstrap)
set(BPFOBJECT_VMLINUX_H "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_ARCH}/vmlinux.h")
set(BPF_ADDITIONAL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")
include(FindBpfObject)

# Build object skeleton and depend skeleton on libbpf build
bpf_object(ananicy_cpp_ng src/ananicy_cpp_ng.bpf.c)
if(BPF_BUILD_LIBBPF)
  add_dependencies(ananicy_cpp_ng_skel libbpf)
endif()
if(BPF_BUILD_BPFTOOL)
  add_dependencies(ananicy_cpp_ng_skel bpftool)
endif()

add_library(ananicy_cpp_ng_bpf_c STATIC
  src/bpf_program_utils.c
  src/btf_helpers.c
  src/trace_helpers.c
  src/uprobe_helpers.c
)
target_include_directories(ananicy_cpp_ng_bpf_c PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ananicy_cpp_ng_bpf_c PRIVATE ananicy_cpp_ng_skel)
add_library(ananicy_cpp_ng_bpf_c::ananicy_cpp_ng_bpf_c ALIAS ananicy_cpp_ng_bpf_c)
if(BPF_BUILD_LIBBPF)
  target_link_libraries(ananicy_cpp_ng_bpf_c PRIVATE bpf::bpf)
endif()

add_library(ananicy_cpp_ng_bpf_cpp INTERFACE)
if(BPF_BUILD_LIBBPF)
  target_include_directories(ananicy_cpp_ng_bpf_cpp INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/libbpf>)
endif()
target_include_directories(ananicy_cpp_ng_bpf_cpp INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include_cpp>)
add_dependencies(ananicy_cpp_ng_bpf_cpp ananicy_cpp_ng_bpf_c)
add_library(ananicy_cpp_ng_bpf_cpp::ananicy_cpp_ng_bpf_cpp ALIAS ananicy_cpp_ng_bpf_cpp)


if(BPF_BUILD_SAMPLES)
  add_executable(runqslower_cpp main.cpp)
  target_compile_features(runqslower_cpp PUBLIC cxx_std_20)
  target_compile_definitions(runqslower_cpp PUBLIC -DSPDLOG_FMT_EXTERNAL -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
  target_link_libraries(runqslower_cpp PRIVATE spdlog fmt ananicy_cpp_ng_bpf_cpp::ananicy_cpp_ng_bpf_cpp ananicy_cpp_ng_bpf_c::ananicy_cpp_ng_bpf_c)

  add_executable(runqslower_c runqslower.c)
  target_link_libraries(runqslower_c PRIVATE ananicy_cpp_ng_bpf_c::ananicy_cpp_ng_bpf_c)
endif()
