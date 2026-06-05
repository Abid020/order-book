cmake_minimum_required(VERSION 3.26)
project(order_book VERSION 0.1.0 LANGUAGES CXX)

# ─────────────────────────────────────────────
#  Global C++ standard
# ─────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────
#  Default build type
# ─────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─────────────────────────────────────────────
#  Compiler flags
# ─────────────────────────────────────────────
add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wno-unused-parameter
  -include new
  $<$<CONFIG:Release>:-O3>
  $<$<CONFIG:Release>:-march=native>   # enables rdtsc, AVX etc. on your local CPU
  $<$<CONFIG:Debug>:-O0>
  $<$<CONFIG:Debug>:-g3>
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_link_options(
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# ─────────────────────────────────────────────
#  FetchContent
# ─────────────────────────────────────────────
include(FetchContent)

# ── Google Test ──────────────────────────────
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.13.0
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Google Benchmark ─────────────────────────
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.4
)
set(BENCHMARK_ENABLE_TESTING       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# ── Dear ImGui ───────────────────────────────
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.90.4
)
FetchContent_MakeAvailable(imgui)

# ── implot ───────────────────────────────────
FetchContent_Declare(
  implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG        v0.16
)
FetchContent_MakeAvailable(implot)

# ─────────────────────────────────────────────
#  System dependencies
# ─────────────────────────────────────────────
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
cmake_policy(SET CMP0167 NEW)
find_package(Boost REQUIRED)

# ─────────────────────────────────────────────
#  ImGui static library
#  (ImGui ships sources, not a CMake target)
# ─────────────────────────────────────────────
add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(imgui_lib PUBLIC
  ${SDL2_LIBRARIES}
  OpenGL::GL
)

# ─────────────────────────────────────────────
#  implot static library
# ─────────────────────────────────────────────
add_library(implot_lib STATIC
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot_lib PUBLIC
  ${implot_SOURCE_DIR}
)
target_link_libraries(implot_lib PUBLIC imgui_lib)

# ─────────────────────────────────────────────
#  Core library  (parser + book + matching)
#  Compiled once, linked by main, tests, bench
# ─────────────────────────────────────────────
add_library(order_book_core STATIC
  src/feed/itch_parser.cpp
  src/book/order_book.cpp
)
target_include_directories(order_book_core PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${Boost_INCLUDE_DIRS}
)
target_compile_features(order_book_core PUBLIC cxx_std_20)

# ─────────────────────────────────────────────
#  Main executable  (visualiser entry point)
# ─────────────────────────────────────────────
add_executable(order_book_app
  src/main.cpp
)
target_link_libraries(order_book_app PRIVATE
  order_book_core
  imgui_lib
  implot_lib
  ${SDL2_LIBRARIES}
  OpenGL::GL
)

# ─────────────────────────────────────────────
#  Tests
# ─────────────────────────────────────────────
enable_testing()

add_executable(order_book_tests
  tests/test_itch_parser.cpp
  tests/test_order_book.cpp
)
target_link_libraries(order_book_tests PRIVATE
  order_book_core
  GTest::gtest_main
  GTest::gmock_main
)
include(GoogleTest)
gtest_discover_tests(order_book_tests)

# ─────────────────────────────────────────────
#  Benchmarks
# ─────────────────────────────────────────────
add_executable(order_book_bench
  bench/bench_order_book.cpp
)
target_link_libraries(order_book_bench PRIVATE
  order_book_core
  benchmark::benchmark_main
)

# ─────────────────────────────────────────────
#  CMake presets support  (optional convenience)
# ─────────────────────────────────────────────
# To build:
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j$(nproc)
#
# To run tests:
#   ctest --test-dir build --output-on-failure
#
# To run benchmarks:
#   ./build/order_book_bench --benchmark_format=json
