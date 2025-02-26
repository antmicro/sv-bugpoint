// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <slang/syntax/SyntaxTree.h>
#include <chrono>
#include <string>

using namespace slang::ast;
using namespace slang::syntax;
using namespace slang;

class SvBugpoint;

std::string prettifyNodeTypename(const char* type);
// stringize type of node, demangle and remove namespace specifier
#define STRINGIZE_NODE_TYPE(TYPE) prettifyNodeTypename(typeid(TYPE).name())

class AttemptStats {
   public:
    std::string pass;
    std::string stage;
    int linesBefore;
    int linesAfter;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
    bool committed;
    std::string typeInfo;
    int idx;

    AttemptStats(const std::string& pass, const std::string& stage, SvBugpoint* svBugpoint)
        : pass(pass), stage(stage), committed(false), svBugpoint(svBugpoint) {}

    AttemptStats& begin();
    AttemptStats& end(bool committed);
    std::string toStr() const;
    void report();
    static void writeHeader(std::string traceFilePath);

   private:
    SvBugpoint* svBugpoint;
};

std::string toString(SourceRange sourceRange);

void copyFile(const std::string& from, const std::string& to);
void mkdir(const std::string& path);
int countLines(const std::string& filename);

std::string prefixLines(const std::string& str, const std::string& linePrefix);
void printSyntaxTree(const std::shared_ptr<SyntaxTree>& tree, std::ostream& file);
void printAst(const RootSymbol& root, std::ostream& file);

// NOTE: doing it as variadic func rather than macro would prevent
// compiler from issuing warnings about incorrect format string
#define PRINTF_ERR(...) \
    do { \
        fprintf(stderr, "sv-bugpoint: "); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

#define PRINTF_INTERNAL_ERR(...) \
    do { \
        PRINTF_ERR("Internal error: %s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            PRINTF_INTERNAL_ERR("Assertion `%s` failed: %s\n", #cond, msg); \
            exit(1); \
        } \
    } while (0)
