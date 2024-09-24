// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <slang/syntax/SyntaxTree.h>
#include <chrono>
#include <string>

using namespace slang::ast;
using namespace slang::syntax;
using namespace slang;

std::string prettifyNodeTypename(const char* type);
// stringize type of node, demangle and remove namespace specifier
#define STRINGIZE_NODE_TYPE(TYPE) prettifyNodeTypename(typeid(TYPE).name())

std::string toString(SourceRange sourceRange);
void dumpTrees();

struct Paths {
    std::string outDir;
    std::string checkScript;
    std::string input;
    std::string output;
    std::string tmpOutput;
    std::string trace;
    std::string dumpSyntax;
    std::string dumpAst;
    std::string intermediateDir;
    Paths() {}
    Paths(std::string outDir, std::string checkScript, std::string input)
        : outDir(outDir), input(input), checkScript(checkScript) {
        output = outDir + "/sv-bugpoint-minimized.sv";
        tmpOutput = outDir + "/sv-bugpoint-tmp.sv";
        trace = outDir + "/sv-bugpoint-trace";
        dumpSyntax = outDir + "/sv-bugpoint-dump-syntax";
        dumpAst = outDir + "/sv-bugpoint-dump-ast";
        intermediateDir = outDir + "/intermediates/";
        if (this->checkScript.find("/") == std::string::npos) {
            // check script is fed to execv that may need this (it is implementation-defined what
            // happens when there is no slash)
            this->checkScript = "./" + checkScript;
        }
    }
};

extern Paths paths;
// Flag for saving intermediate output of each attempt
extern bool saveIntermediates;
// Global counter incremented after end of each attempt
// Meant mainly for setting up conditional breakpoints based on trace
extern int currentAttemptIdx;

void copyFile(const std::string& from, const std::string& to);
void mkdir(const std::string& path);
int countLines(const std::string& filename);

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

    AttemptStats(const std::string& pass, const std::string& stage)
        : pass(pass), stage(stage), committed(false) {}

    AttemptStats& begin();
    AttemptStats& end(bool committed);
    std::string toStr() const;
    void report();
    static void writeHeader();
};

bool test(AttemptStats& stats);
bool test(std::shared_ptr<SyntaxTree>& tree, AttemptStats& info);
