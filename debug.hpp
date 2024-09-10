#pragma once
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <iostream>
#include <string>

using namespace slang::ast;
using namespace slang::syntax;
using namespace slang;

#ifdef __GLIBCXX__
#include <cxxabi.h>
inline std::string tryDemangle(const char* mangled) {
    int rc;
    char* out = abi::__cxa_demangle(mangled, NULL, NULL, &rc);
    if (rc != 0) {
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
    while ((pos = src.find(pattern)) != std::string::npos) {
        src.erase(pos, pattern.length());
    }
    return src;
}

inline std::string prettifyNodeTypename(const char* type) {
    // demangle and remove namespace specifier from stringized node type
    std::string demangled = tryDemangle(type);
    return removeAll(removeAll(demangled, "slang::syntax::"), "slang::ast::");
}

// stringize type of node, demangle and remove namespace specifier
#define STRINGIZE_NODE_TYPE(TYPE) prettifyNodeTypename(typeid(TYPE).name())

#define DERIVED static_cast<TDerived*>(this)
template <typename TDerived>
class TreePrinter : public SyntaxVisitor<TDerived> {
   private:
    int indentLevel;
    int minLinesFilter;
    std::ostream& out;

   public:
    TreePrinter(int minLinesFilter = 0, std::ostream& out = std::cout) : out(out) {
        indentLevel = 0;
        this->minLinesFilter = minLinesFilter;
    }

    void printIndent() {
        for (int i = 0; i < indentLevel; ++i)
            out << " ";
    }

    template <typename T>
    void tryPrintNode(const T& node) {
        int lines = std::ranges::count(node.toString(), '\n') + 1;
        if (lines >= minLinesFilter) {
            indentLevel++;
            printIndent();
            out << STRINGIZE_NODE_TYPE(T) << ", " << node.kind << ", lines: " << lines << "\n";
            out << node.toString() << "\n";
            DERIVED->visitDefault(node);
            indentLevel--;
        } else {
            DERIVED->visitDefault(node);
        }
    }
};

class AllSyntaxPrinter : public TreePrinter<AllSyntaxPrinter> {
   public:
    AllSyntaxPrinter(int minLinesFilter = 0, std::ostream& out = std::cout)
        : TreePrinter<AllSyntaxPrinter>(minLinesFilter, out) {}

    template <typename T>
    void handle(const T& node) {
        tryPrintNode(node);
    }
};

class StatementSyntaxPrinter : public TreePrinter<StatementSyntaxPrinter> {
   public:
    StatementSyntaxPrinter(int minLinesFilter = 0, std::ostream& out = std::cout)
        : TreePrinter<StatementSyntaxPrinter>(minLinesFilter, out) {}

    void handle(const StatementSyntax& node) { tryPrintNode(node); }
};

class AstPrinter : public ASTVisitor<AstPrinter, true, true, true> {
   private:
    std::ostream& out;

   public:
    AstPrinter(std::ostream& out = std::cout) : out(out) {}

    template <typename T>
    void handle(const T& node) {
        out << "node: " << STRINGIZE_NODE_TYPE(T) << " " << toString(node.kind) << " "
            << "\n";
        if constexpr (requires { node.getSyntax(); }) {
            if (node.getSyntax())
                out << node.getSyntax()->toString() << "\n";
        }
        visitDefault(node);
    }
};

inline std::string toString(SourceRange sourceRange) {
    if (sourceRange == SourceRange::NoLocation)
        return "NO_LOCATION";
    else
        return "buffer:" + std::to_string(sourceRange.start().buffer().getId()) +
               ", offsetStart: " + std::to_string(sourceRange.start().offset()) +
               ", offsetEnd: " + std::to_string(sourceRange.end().offset());
}
