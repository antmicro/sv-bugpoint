cmake_minimum_required(VERSION 3.20)
project(bugpoint)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_TAG f6e351086d5209f4aa518ec01647eb9208fc7912
)
FetchContent_MakeAvailable(slang)

add_executable(bugpoint bugpoint.cpp)
target_link_libraries(bugpoint PRIVATE slang::slang)
