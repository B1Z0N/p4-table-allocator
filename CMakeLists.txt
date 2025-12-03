cmake_minimum_required(VERSION 3.24)
project(p4_table_allocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
FetchContent_Declare(json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.11.3
    URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406)
FetchContent_MakeAvailable(json)

add_library(project_options INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(project_options INTERFACE -Wall -Wextra -Werror -Wpedantic)
endif()
add_library(p4alloc src/input.cpp src/graph.cpp src/allocator.cpp)
target_include_directories(p4alloc PUBLIC include)
target_link_libraries(p4alloc PUBLIC nlohmann_json::nlohmann_json project_options)

include(CTest)
if(BUILD_TESTING)
    FetchContent_Declare(Catch2
        URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.7.1
        URL_HASH SHA256=c991b247a1a0d7bb9c39aa35faf0fe9e19764213f28ffba3109388e62ee0269c)
    FetchContent_MakeAvailable(Catch2)
    add_executable(allocator_tests tests/input_test.cpp tests/graph_test.cpp tests/allocator_test.cpp)
    target_link_libraries(allocator_tests PRIVATE p4alloc Catch2::Catch2WithMain)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(allocator_tests)
endif()
