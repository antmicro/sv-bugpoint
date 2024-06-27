cmake_minimum_required(VERSION 3.20)
project(example)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Note: this example just pulls the head master branch
# for slang, but a real project would very likely pin
# this to a specific tag and only update when ready
# to accept a new version.
include(FetchContent)
FetchContent_Declare(
  slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(slang)

add_executable(example example.cpp)
target_link_libraries(example PRIVATE slang::slang)
