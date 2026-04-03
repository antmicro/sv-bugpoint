cmake_minimum_required(VERSION 3.20)
project(sv-bugpoint)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_TAG 3d7e6cd2e213e62480b2afcba677a3d12aa4e162
  # Slang breaks on Boost 1.89.0, patch to use exact version matching
  PATCH_COMMAND git checkout . && git apply ${CMAKE_CURRENT_LIST_DIR}/external/0001-slang-find-exact-boost-version.patch
)
FetchContent_MakeAvailable(slang)

add_executable(sv-bugpoint
  source/SvBugpoint.cpp
  source/Utils.cpp
  source/SetRemovers.cpp
  source/BodyRemover.cpp
  source/LabelRemover.cpp
  source/BodyPartsRemover.cpp
  source/DeclRemover.cpp
  source/InstantationRemover.cpp
  source/BindRemover.cpp
  source/ModportRemover.cpp
  source/ContAssignRemover.cpp
  source/ParamAssignRemover.cpp
  source/ModuleRemover.cpp
  source/StatementsRemover.cpp
  source/MemberRemover.cpp
  source/ImportsRemover.cpp
  source/TypeSimplifier.cpp
)

target_link_libraries(sv-bugpoint PRIVATE slang::slang)
target_precompile_headers(sv-bugpoint PUBLIC
  <string>
  <filesystem>
  <fstream>
  <chrono>
  <iostream>
  <unordered_set>
  <slang/syntax/SyntaxVisitor.h>
  <slang/syntax/SyntaxTree.h>
  <slang/syntax/AllSyntax.h>
  <slang/ast/ASTVisitor.h>
  <slang/syntax/SyntaxPrinter.h>
  <slang/text/SourceLocation.h>
)

if (NOT MSVC)
    target_compile_options(sv-bugpoint PRIVATE -Wall -Wextra -pedantic -Wno-reorder -Wno-unused-parameter)
    if(DEFINED ENV{CI})
      target_compile_options(sv-bugpoint PRIVATE -Werror)
    endif()
endif()

install(TARGETS sv-bugpoint RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
