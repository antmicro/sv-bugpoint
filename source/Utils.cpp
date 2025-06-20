// SPDX-License-Identifier: Apache-2.0
#include "Utils.hpp"
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include "SvBugpoint.hpp"

#ifdef __GLIBCXX__
#include <cxxabi.h>
std::string tryDemangle(const char* mangled) {
    int rc;
    char* out = abi::__cxa_demangle(mangled, NULL, NULL, &rc);
    ASSERT(rc == 0, "demangling failed");
    std::string outStr = out;
    free(out);
    return outStr;
}
#else
std::string tryDemangle(const char* mangled) {
    return mangled;
}
#endif

// remove all occurrences of pattern from src string
std::string removeAll(std::string src, std::string pattern) {
    size_t pos;
    while ((pos = src.find(pattern)) != std::string::npos) {
        src.erase(pos, pattern.length());
    }
    return src;
}

std::string prettifyNodeTypename(const char* type) {
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

void printSyntaxTree(const std::shared_ptr<SyntaxTree>& tree, std::ostream& file) {
    AllSyntaxPrinter(0, file).visit(tree->root());
}

void printAst(const RootSymbol& root, std::ostream& file) {
    AstPrinter(file).visit(root);
}

std::string toString(SourceRange sourceRange) {
    if (sourceRange == SourceRange::NoLocation)
        return "NO_LOCATION";
    else
        return "buffer:" + std::to_string(sourceRange.start().buffer().getId()) +
               ", offsetStart: " + std::to_string(sourceRange.start().offset()) +
               ", offsetEnd: " + std::to_string(sourceRange.end().offset());
}

void copyFile(const std::string& from, const std::string& to) {
    if (from == to) {
        return;
    }
    try {
        std::filesystem::copy(from, to, std::filesystem::copy_options::overwrite_existing);
    } catch (const std::filesystem::filesystem_error& err) {
        PRINTF_ERR("failed to copy '%s' to '%s': %s\n", from.c_str(), to.c_str(),
                   err.code().message().c_str());
        exit(1);
    }
}

void mkdir(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
    } catch (const std::filesystem::filesystem_error& err) {
        PRINTF_ERR("failed to make directory '%s': %s\n", path.c_str(),
                   err.code().message().c_str());
        exit(1);
    }
}

int countLines(const std::string& filename) {
    std::ifstream file(filename);
    return std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
}

AttemptStats& AttemptStats::begin() {
    linesBefore = countLines(svBugpoint->getMinimizedFile());
    startTime = std::chrono::high_resolution_clock::now();
    idx = svBugpoint->getCurrentAttemptIdx();
    return *this;
}
AttemptStats& AttemptStats::end(bool committed) {
    this->committed = committed;
    linesAfter = countLines(svBugpoint->getTmpFile());
    endTime = std::chrono::high_resolution_clock::now();
    if (svBugpoint->getSaveIntermediates()) {
        copyFile(svBugpoint->getTmpFile(), svBugpoint->getAttemptOutput());
    }
    svBugpoint->updateCurrentAttemptIdx();
    return *this;
}

std::string AttemptStats::toStr() const {
    std::stringstream tmp;
    int lines = linesBefore - linesAfter;
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    tmp << pass << '\t' << stage << '\t' << lines << '\t' << committed << '\t' << duration << "ms\t"
        << idx << '\t' << typeInfo << '\t' << svBugpoint->getOriginalFile() << "\n";
    return tmp.str();
}

void AttemptStats::report() {
    std::cerr << toStr();
    std::ofstream file(svBugpoint->getTraceFile(), std::ios_base::app);
    file << toStr() << std::flush;
}

void AttemptStats::writeHeader(std::string traceFilePath) {
    std::ofstream file(traceFilePath);
    file << "pass\tstage\tlines_removed\tcommitted\ttime\tidx\ttype_info\tinput_file\n";
}

std::string prefixLines(const std::string& str, const std::string& linePrefix) {
    std::istringstream sstream(str);
    std::string line, out;
    while (getline(sstream, line)) {
        out += linePrefix + line + '\n';
    }
    return out;
}
