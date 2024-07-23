#pragma once
#include <string>
#include <iostream>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>

using namespace slang::syntax;
using namespace slang;

// This is non-portable. TODO: ifdef it or something
#include <cxxabi.h>
inline std::string demangle(const char* mangled) {
  int rc;
  char* out = abi::__cxa_demangle(mangled, NULL, NULL, &rc);
  if(rc != 0) {
    std::cerr << "demangling error!\n";
    exit(1);
  }
  std::string outStr = out;
  free(out);
  return outStr;
}

inline std::string removeAllNeddles(std::string haystack, std::string needle) {
  size_t pos;
  while((pos = haystack.find(needle)) != std::string::npos) {
    haystack.erase(pos, needle.length());
  }
  return haystack;
}

inline std::string prettifyNodeTypename(const char* type) {
  // stringize type of node, demangle and remove namespace specifier
  std::string demangled = demangle(type);
  return removeAllNeddles(demangled, "slang::syntax::");
}

#define DERIVED static_cast<TDerived*>(this)
template<typename TDerived>
class TreePrinter: public SyntaxVisitor<TDerived> {
  private:
    int indentLevel;
    int minLinesFilter;
  public:

  TreePrinter(int minLinesFilter = 0) {
    indentLevel = 0;
    this->minLinesFilter = minLinesFilter;
  }

  void printIndent() {
    for(int i = 0; i<indentLevel; ++i) std::cout<<" ";
  }

  template<typename T>
  void tryPrintNode(const T& node) {
      int lines = std::ranges::count(node.toString(), '\n') + 1;
      if(lines >= minLinesFilter) {
        indentLevel++;
        printIndent();
        std::string type = prettifyNodeTypename(typeid(node).name());
        std::cout << type << ", " << node.kind << ", lines: " << lines << "\n";
        DERIVED->visitDefault(node);
        indentLevel--;
      } else {
        DERIVED->visitDefault(node);
      }
  }
};

class AllPrinter: public TreePrinter<AllPrinter> {
  public:
  template<typename T>
  void handle(const T& node) {
    tryPrintNode(node);
  }
};

class StatementPrinter: public TreePrinter<StatementPrinter> {
  public:
  void handle(const StatementSyntax& node) {
    tryPrintNode(node);
  }
};
