cmake_minimum_required(VERSION 3.20)
project(sv-bugpoint)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_TAG 0ef0ac467cbd0ca11baf91398f8699bd96bcf4ff
)
FetchContent_MakeAvailable(slang)

add_executable(sv-bugpoint sv-bugpoint.cpp utils.cpp PairRemovers.cpp BodyRemover.cpp
  BodyPartsRemover.cpp DeclRemover.cpp InstantationRemover.cpp ModportRemover.cpp ContAssignRemover.cpp ParamAssignRemover.cpp
  StatementsRemover.cpp MemberRemover.cpp ImportsRemover.cpp)

target_link_libraries(sv-bugpoint PRIVATE slang::slang)
target_precompile_headers(sv-bugpoint PUBLIC
  <string>
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
