message(STATUS "Configuring google benchmark")
set(CMAKE_MESSAGE_CONTEXT "google::benchmark")

set(BENCHMARK_ENABLE_INSTALL OFF CACHE INTERNAL "" FORCE)
set(BENCHMARK_INSTALL_DOCS   OFF CACHE INTERNAL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE INTERNAL "" FORCE)

CPMAddPackage(
   NAME benchmark
   GITHUB_REPOSITORY google/benchmark
   GIT_PROGRESS TRUE
   GIT_SHALLOW TRUE
   GIT_TAG v1.7.1
   EXCLUDE_FROM_ALL YES
)
set(CMAKE_MESSAGE_CONTEXT "")

# benchmark binary
add_executable(ananicy_cpp_benchmarks src/benchmarks.cpp)
target_compile_features(ananicy_cpp_benchmarks PRIVATE cxx_std_20)
target_link_libraries(ananicy_cpp_benchmarks benchmark::benchmark tbb ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(ananicy_cpp_benchmarks PRIVATE ${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_DIR})
