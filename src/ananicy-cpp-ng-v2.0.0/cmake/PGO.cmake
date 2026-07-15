option(ENABLE_PGO_BUILD "Enable ananicy-cpp-ng PGO" "")
option(PROFILES_PGO_DIR "Profile Folder for PGO" "${CMAKE_SOURCE_DIR}/pgo-data")

set(ENABLE_PGO_BUILD_VALUES "instrument" "use")
set_property(CACHE ENABLE_PGO_BUILD PROPERTY STRINGS ${ENABLE_PGO_BUILD_VALUES})
list(
    FIND
    ENABLE_PGO_BUILD_VALUES
    ${ENABLE_PGO_BUILD}
    ENABLE_PGO_BUILD_INDEX)

if(NOT ENABLE_PGO_BUILD OR ENABLE_PGO_BUILD STREQUAL "")
    return()
endif()

if(${ENABLE_PGO_BUILD_INDEX} EQUAL -1)
    message(
        STATUS
        "Unknown PGO mode: '${ENABLE_PGO_BUILD}', available modes are ${ENABLE_PGO_BUILD_VALUES}")
endif()

if(ENABLE_PGO_BUILD STREQUAL "instrument")

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-generate=${PROFILES_PGO_DIR}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-dir=${PROFILES_PGO_DIR} -fprofile-generate=${PROFILES_PGO_DIR}")
endif()

elseif(ENABLE_PGO_BUILD STREQUAL "use")

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-use=${PROFILES_PGO_DIR} -Wno-backend-plugin")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-dir=${PROFILES_PGO_DIR} -fprofile-use=${PROFILES_PGO_DIR} -fprofile-correction")
endif()

endif()


if(NOT EXISTS ${PROFILES_PGO_DIR})
  file(MAKE_DIRECTORY ${PROFILES_PGO_DIR})
endif()
