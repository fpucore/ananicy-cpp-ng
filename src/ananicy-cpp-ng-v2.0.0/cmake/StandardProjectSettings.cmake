# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE
      RelWithDebInfo
      CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui, ccmake
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS
             "Debug"
             "Release"
             "MinSizeRel"
             "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -fconcepts")
    endif()
endif()
set(CMAKE_MESSAGE_CONTEXT_SHOW ON)

# Generate compile_commands.json to make it easier to work with clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_EXTERNAL_JSON "Use an external JSON library" OFF)
option(USE_EXTERNAL_SPDLOG "Use external Spdlog library" OFF)
option(USE_EXTERNAL_FMTLIB "Use external fmt library" OFF)
option(ENABLE_SYSTEMD "Include systemd headers and install service file" ON)
option(STATIC "Build static binary" OFF)
option(OPTIMIZE_FOR_NATIVE_MICROARCH "Enable -march=native flag on the CXX compiler. This makes build NOT portable." OFF)
option(USE_EXPERIMENTAL_IMPL "Use experimental impl" ON)
option(USE_BPF_PROC_IMPL "Use BPF as a proc listener impl" OFF)
option(ENABLE_ANANICY_NG_TESTS "Build ananicy-cpp-ng tests" OFF)
option(ENABLE_ANANICY_NG_FUZZ_TESTS "Build ananicy-cpp-ng fuzz tests" OFF)
option(ENABLE_ANANICY_NG_BENCHMARKS "Build ananicy-cpp-ng benchmark" OFF)
option(BUILD_WITH_RELOCS "Build ananicy-cpp-ng with relocs included" OFF)

option(ENABLE_IPO "Enable Interprocedural Optimization, aka Link Time Optimization (LTO)" OFF)

if(ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(
    RESULT
    result
    OUTPUT
    output)
  if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(SEND_ERROR "IPO is not supported: ${output}")
  endif()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  add_compile_options(-fcolor-diagnostics)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
else()
  message(STATUS "No colored compiler diagnostic set for '${CMAKE_CXX_COMPILER_ID}' compiler.")
endif()

# Enables STL container checker if not building a release.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-D_GLIBCXX_ASSERTIONS)
endif()

# Enables compilation with static linking
if(STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    set(BUILD_SHARED_LIBS OFF)
    if (ENABLE_SYSTEMD)
        message(FATAL_ERROR "Can't make a static build with systemd enabled, add -DENABLE_SYSTEMD=Off to cmake's invocation")
    endif()
    if(OPTIMIZE_FOR_NATIVE_MICROARCH)
        message(WARNING "It is not recommended to build a static binary with -march=native")
    endif()
endif()

# Experimental impl
if(USE_EXPERIMENTAL_IMPL)
  add_definitions(-DUSE_EXPERIMENTAL_IMPL)
endif()

# Use BPF as a proc listener
if(USE_BPF_PROC_IMPL)
  add_definitions(-DUSE_BPF_PROC_IMPL)
endif()

# Build ananicy-cpp-ng with relocs included
if(BUILD_WITH_RELOCS)
  add_link_options(-Wl,--emit-relocs)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fno-reorder-blocks-and-partition)
  endif()
endif()
