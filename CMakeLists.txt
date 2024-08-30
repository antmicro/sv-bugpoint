cmake_minimum_required(VERSION 3.20)
project(bugpoint)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_TAG 0ef0ac467cbd0ca11baf91398f8699bd96bcf4ff
)
FetchContent_MakeAvailable(slang)

add_executable(bugpoint bugpoint.cpp)
target_link_libraries(bugpoint PRIVATE slang::slang)
