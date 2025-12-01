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
add_library(p4alloc src/input.cpp)
target_include_directories(p4alloc PUBLIC include)
target_link_libraries(p4alloc PUBLIC nlohmann_json::nlohmann_json project_options)
