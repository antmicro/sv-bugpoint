// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <slang/syntax/SyntaxTree.h>
#include <slang/util/CommandLine.h>
#include <string>
#include "Utils.hpp"

namespace fs = std::filesystem;

class SvBugpoint {
   public:
    SvBugpoint() : currentPathIdx(0), currentAttemptIdx(0) {}

    void addArgs();
    void usage();
    void parseCommandLine(std::string_view argList, CommandLine::ParseOptions parseOptions);
    void parseCommandLine(int argc, char** argv);
    void processCommandFiles(fs::path file);
    void addFilesFromDirectory(fs::path dir);
    void parseArgs(int argc, char** argv);

    void addPath(std::string_view path) { paths.emplace_back(path); }
    bool getSaveIntermediates() { return saveIntermediates.value_or(false); }
    int getCurrentAttemptIdx() { return currentAttemptIdx; }
    void updateCurrentAttemptIdx() { currentAttemptIdx++; }
    void updateCurrentAttemptIdx(int idx) { currentAttemptIdx = idx; }
    void setCurrentAttemptIdx(int idx) { currentAttemptIdx = idx; }
    void dryRun();
    void initOutDir();
    void removeVerilatorConfig();
    void minimize();
    bool pass(std::shared_ptr<SyntaxTree>& tree, const std::string& passIdx = "-");

    bool test(AttemptStats& stats);
    bool test(std::shared_ptr<SyntaxTree>& tree, AttemptStats& stats);
    void checkDumpTrees();

    fs::path getOutDir() { return outDir; }
    fs::path getTmpDir() { return outDir / kTmpDir; }
    fs::path getInputStem() { return getInput().stem(); }
    fs::path getInputExtension() { return getInput().extension(); }
    fs::path getIntermediateDir() { return getOutDir() / kIntermediatesDir; }
    fs::path getInput() { return paths[currentPathIdx]; }
    fs::path getOutput() { return getOutputFileName(kSvBugpointPrefix, ""); }
    fs::path getTmpOutput() {
        fs::path result = getTmpDir() / getInputStem();
        result += getInputExtension();
        return result;
    }
    fs::path getTrace() { return getOutputFileName(kTracePrefix, ""); }
    fs::path getDumpSyntax() { return getOutputFileName(kSvBugpointPrefix, kDumpSyntaxSuffix); }
    fs::path getDumpAst() { return getOutputFileName(kSvBugpointPrefix, kDumpAstSuffix); }
    fs::path getMinimalizedOutput() { return getOutDir() / kSvBugpointMinimalized; }
    fs::path getAttemptOutput() {
        fs::path result = getIntermediateDir() / "attempt";
        result += std::to_string(currentAttemptIdx);
        result += getInputExtension();
        return result;
    }
    fs::path getCheckScript() { return checkScript; }
    std::vector<std::string> getTestArgs() {
        std::vector<std::string> result;
        auto oldCurrentPathIdx = currentPathIdx;
        for (size_t i = 0; i < paths.size(); i++) {
            currentPathIdx = i;
            if ((int)i != oldCurrentPathIdx) {
                result.push_back(getOutput());
            }
        }
        currentPathIdx = oldCurrentPathIdx;
        result.push_back(getTmpOutput());
        return result;
    }

    void saveMinimalizedFile();

   private:
    const fs::path kTmpDir{"tmp"};
    const fs::path kIntermediatesDir{"intermediates"};
    const fs::path kSvBugpointPrefix{"sv-bugpoint-"};
    const fs::path kTracePrefix{"sv-bugpoint-trace"};
    const fs::path kDumpSyntaxSuffix{"dump-syntax"};
    const fs::path kDumpAstSuffix{"dump-ast"};
    const fs::path kSvBugpointMinimalized{"sv-bugpoint-minimized.sv"};

    fs::path getOutputFileName(const fs::path& prefix, const fs::path& suffix) {
        fs::path result = getOutDir() / prefix;
        result += getInputStem();
        result += suffix;
        result += getInputExtension();
        return result;
    };
    CommandLine cmdLine;

    std::vector<fs::path> paths;
    int currentPathIdx;
    // Global counter incremented after end of each attempt
    // Meant mainly for setting up conditional breakpoints based on trace
    int currentAttemptIdx;
    std::string checkScript;
    std::optional<bool> dump;
    std::optional<bool> force;
    // Flag for saving intermediate output of each attempt
    std::optional<bool> saveIntermediates;
    std::optional<bool> showHelp;
    std::string outDir;
    flat_hash_set<fs::path> activeCommandFiles;
};
