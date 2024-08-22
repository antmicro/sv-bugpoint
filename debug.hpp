#pragma once
#include <string>
#include <iostream>
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>

using namespace slang::ast;
using namespace slang::syntax;
using namespace slang;

#ifdef __GLIBCXX__
#include <cxxabi.h>
inline std::string tryDemangle(const char* mangled) {
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
#else
inline std::string tryDemangle(const char* mangled) {
  return mangled;
}
#endif

// remove all occurrences of pattern from src string
inline std::string removeAll(std::string src, std::string pattern) {
  size_t pos;
  while((pos = src.find(pattern)) != std::string::npos) {
    src.erase(pos, pattern.length());
  }
  return src;
}

inline std::string prettifyNodeTypename(const char* type) {
  // demangle and remove namespace specifier from stringized node type
  std::string demangled = tryDemangle(type);
  return removeAll(demangled, "slang::syntax::");
}

// stringize type of node, demangle and remove namespace specifier
#define STRINGIZE_NODE_TYPE(TYPE) prettifyNodeTypename(typeid(TYPE).name())

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
        std::cout << STRINGIZE_NODE_TYPE(T) << ", " << node.kind << ", lines: " << lines << "\n";
        std::cout << node.toString() << "\n";
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


class AstPrinter : public ASTVisitor<AstPrinter, true, true, true> {
  public:
      template <typename T>
      void handle(const T& node) {
      std::cerr <<"node: " << typeid(T).name() << " " <<toString(node.kind) << " "<< "\n";
      if constexpr (requires { node.getSyntax(); }) {
        if(node.getSyntax()) std::cerr << node.getSyntax()->toString() << "\n";
      }
      visitDefault(node);
  }
};
