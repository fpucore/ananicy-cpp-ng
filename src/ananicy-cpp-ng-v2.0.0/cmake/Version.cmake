set(VERSION "0.0.0" CACHE STRING "Override this value to specify the version string used by CMake and Ananicy-cpp")
set(COMMIT_HASH "")
set(COMMIT_COUNT 0)
set(FULL_VERSION "${VERSION}")

if(VERSION STREQUAL "0.0.0")
    message(STATUS "No version defined, fetching one from git")
    find_package(Git)
    if(Git_FOUND)
        message(STATUS "Git found: ${GIT_EXECUTABLE}")

        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --always
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE VERSION_STRING
            RESULT_VARIABLE STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(${VERSION_STRING} MATCHES ".+-([0-9]+-[0-9a-z]+)?-?(dirty)?$")
            message(STATUS "VERSION_STRING: ${VERSION_STRING}, did match")
            string(REGEX REPLACE "v([0-9A-Za-z.-]+)-([0-9]+)-([a-zA-Z0-9]+).+" "\\1.r\\2.\\3" FULL_VERSION "${VERSION_STRING}")
            set(VERSION ${FULL_VERSION})
            set(COMMIT_COUNT ${CMAKE_MATCH_2})
            set(COMMIT_HASH ${CMAKE_MATCH_3})
            message(STATUS "Version from git: ${FULL_VERSION}")
        else()
            message(STATUS "VERSION_STRING: ${VERSION_STRING}, did not match")
        endif()
    endif()
endif()
message("Version: ${VERSION}")
string(REGEX REPLACE "([0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9])?)(.*)" "\\1" VERSION_FOR_CMAKE "${VERSION}")
message(STATUS "version for cmake: ${VERSION_FOR_CMAKE}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/version.hpp)
