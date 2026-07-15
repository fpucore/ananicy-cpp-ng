message(STATUS "Configuring doctest")
set(CMAKE_MESSAGE_CONTEXT "doctest")
CPMAddPackage(
   NAME doctest
   GITHUB_REPOSITORY doctest/doctest
   GIT_PROGRESS TRUE
   GIT_SHALLOW TRUE
   GIT_TAG v2.4.9
   EXCLUDE_FROM_ALL YES
)
set(CMAKE_MESSAGE_CONTEXT "")

list(APPEND test_SOURCES
        ${CMAKE_SOURCE_DIR}/src/utils.cpp
        ${CMAKE_SOURCE_DIR}/src/rules.cpp
        ${CMAKE_SOURCE_DIR}/src/worker.cpp
        ${CMAKE_SOURCE_DIR}/src/config.cpp
        ${CMAKE_SOURCE_DIR}/src/utility/argument_parsing/argument_parser.cpp
        ${CMAKE_SOURCE_DIR}/src/utility/argument_parsing/argument.cpp
        ${CMAKE_SOURCE_DIR}/src/platform/linux/priority.cpp
        ${CMAKE_SOURCE_DIR}/src/platform/linux/cgroups.cpp
        ${CMAKE_SOURCE_DIR}/src/platform/linux/process_info.cpp
        ${CMAKE_SOURCE_DIR}/src/platform/linux/process.cpp)

add_library(test_libreq STATIC ${test_SOURCES})
target_include_directories(test_libreq PRIVATE ${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_DIR})
target_link_libraries(test_libreq PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog fmt::fmt)
if(USE_BPF_PROC_IMPL)
    target_link_libraries(test_libreq PRIVATE ananicy_cpp_bpf_cpp::ananicy_cpp_bpf_cpp ananicy_cpp_bpf_c::ananicy_cpp_bpf_c)
else()
    target_link_libraries(test_libreq PRIVATE ananicy_cpp_netlink_cpp::ananicy_cpp_netlink_cpp ananicy_cpp_netlink_c::ananicy_cpp_netlink_c)
endif()
if(ENABLE_SYSTEMD)
    target_link_libraries(test_libreq PRIVATE systemd)
endif()

set(TEST_DIR "\"${PROJECT_SOURCE_DIR}/src/tests/\"")

#############################################################################
# doctest library with the main function to speed up build
#############################################################################

add_library(doctest_main OBJECT unit.cpp)
target_compile_features(doctest_main PUBLIC cxx_std_20)
target_include_directories(doctest_main PRIVATE ${CMAKE_BINARY_DIR}/include ${CMAKE_CURRENT_DIR})
target_link_libraries(doctest_main PRIVATE doctest::doctest)

#############################################################################
# one executable for each unit test file
#############################################################################

file(GLOB files unit-*.cpp)

foreach(file ${files})
    get_filename_component(file_basename ${file} NAME_WE)
    string(REGEX REPLACE "unit-([^$]+)" "test-\\1" testcase ${file_basename})

    add_executable(${testcase} $<TARGET_OBJECTS:doctest_main> ${file})
    target_compile_definitions(${testcase} PRIVATE DOCTEST_CONFIG_SUPER_FAST_ASSERTS)
    target_compile_definitions(${testcase} PRIVATE TEST_DIR=${TEST_DIR})
    target_compile_options(${testcase} PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-deprecated;-Wno-float-equal>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-deprecated-declarations>
    )
    target_include_directories(${testcase} PRIVATE ${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_DIR})
    target_link_libraries(${testcase} PRIVATE test_libreq nlohmann_json::nlohmann_json spdlog::spdlog fmt::fmt doctest::doctest)

    add_test(NAME "${testcase}"
        COMMAND ${testcase} ${DOCTEST_TEST_FILTER} --no-skip
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endforeach()

if(ENABLE_ANANICY_FUZZ_TESTS AND CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    #############################################################################
    # one executable for each fuzz test file
    #############################################################################

    file(GLOB fuzz_files fuzzer-*.cpp)

    foreach(file ${fuzz_files})
        get_filename_component(file_basename ${file} NAME_WE)
        string(REGEX REPLACE "fuzzer-([^$]+)" "fuzz-\\1" testcase ${file_basename})

        add_executable(${testcase} ${file})
        set_target_properties(${testcase} PROPERTIES COMPILE_FLAGS "-g -fsanitize=address,fuzzer")
        set_target_properties(${testcase} PROPERTIES LINK_OPTIONS "-fsanitize=address,fuzzer")
        target_compile_definitions(${testcase} PRIVATE TEST_DIR=${TEST_DIR})
        target_include_directories(${testcase} PRIVATE ${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_DIR})
        target_link_libraries(${testcase} PRIVATE test_libreq nlohmann_json::nlohmann_json spdlog::spdlog fmt::fmt)
    endforeach()
elseif(ENABLE_ANANICY_FUZZ_TESTS)
    message(FATAL_ERROR "Clang compiler is required for fuzz tests")
endif()
