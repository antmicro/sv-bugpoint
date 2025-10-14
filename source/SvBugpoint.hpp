// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <slang/syntax/SyntaxTree.h>
#include <slang/util/CommandLine.h>
#include <string>
#include "Utils.hpp"

namespace fs = std::filesystem;

// simple helper for (re)loading tree and keeping required resources alive until next reload
class TreeLoader {
   public:
    std::shared_ptr<SyntaxTree> load(fs::path file);
    ~TreeLoader();

    TreeLoader() : originalTree(nullptr), sourceManager(nullptr) {}
    TreeLoader(const TreeLoader&) = delete;
    TreeLoader& operator=(const TreeLoader&) = delete;

   private:
    // https://github.com/MikePopoloski/slang/commit/dc010f37a82898e7ca5365d8a32c86f127b49b34
    // is incomplete. Given A->B->C->D chain of tree transforms B,C,D still depend on original
    // A tree. Hence, original tree must be kept alive until next load
    std::shared_ptr<SyntaxTree> originalTree;
    SourceManager* sourceManager;
};

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

    void addPath(std::string_view path) { inputFiles.emplace_back(path); }
    bool getSaveIntermediates() { return saveIntermediates.value_or(false); }
    int getCurrentAttemptIdx() { return currentAttemptIdx; }
    void updateCurrentAttemptIdx() { currentAttemptIdx++; }
    void updateCurrentAttemptIdx(int idx) { currentAttemptIdx = idx; }
    void setCurrentAttemptIdx(int idx) { currentAttemptIdx = idx; }
    void dryRun();
    void initOutDir();
    void removeVerilatorConfig();
    void minimize();
    bool pass(const std::string& passIdx = "-");

    bool test(AttemptStats& stats);
    bool test(std::shared_ptr<SyntaxTree>& tree, AttemptStats& stats);
    void checkDumpTrees();

    fs::path getWorkDir() { return workDir; }
    fs::path getOutDir() { return workDir / "minimized"; }
    fs::path getTmpOutDir() { return workDir / "tmp"; }
    fs::path getDebugDir() { return workDir / "debug"; }
    fs::path getIntermediateDir() { return getDebugDir() / "attempts"; }

    fs::path getOriginalFile() { return inputFiles[currentPathIdx]; }
    fs::path getMinimizedFile() { return minimizedFiles[currentPathIdx]; }
    fs::path getTmpFile() { return tmpFiles[currentPathIdx]; }

    std::string getExtension() { return getOriginalFile().extension(); }
    std::string getStem() { return getOriginalFile().stem(); }
    std::string getBasename() { return getOriginalFile().filename(); }
    std::string getShortPath() {
        // common/work/dir/a.sv -> a.sv
        // common/work/dir/subdir/b.sv -> subdir/b.sv
        return fs::relative(getOriginalFile(), commonInputAncestor);
    }

    fs::path getTraceFile() { return getDebugDir() / "trace"; }
    fs::path getDumpSyntaxFile() { return getDebugDir() / "syntax-dump"; }
    fs::path getDumpAstFile() { return getDebugDir() / "ast-dump"; }
    fs::path getCombinedOutputFile() { return workDir / "sv-bugpoint-combined.sv"; }
    fs::path getAttemptOutput() {
        std::string name =
            getStem() + ".attempt" + std::to_string(currentAttemptIdx) + getExtension();
        return getIntermediateDir() / name;
    }

    fs::path getCheckScript() { return checkScript; }
    std::vector<std::string> getTestArgs() {
        std::vector<std::string> result;
        auto oldCurrentPathIdx = currentPathIdx;
        for (size_t i = 0; i < minimizedFiles.size(); i++) {
            currentPathIdx = i;
            if ((int)i != oldCurrentPathIdx) {
                result.push_back(getMinimizedFile());
            }
        }
        currentPathIdx = oldCurrentPathIdx;
        result.push_back(getTmpFile());
        return result;
    }

    void saveCombinedOutput();

    TreeLoader treeLoader;

   private:
    CommandLine cmdLine;

    fs::path commonInputAncestor;
    std::vector<fs::path> inputFiles;
    std::vector<fs::path> minimizedFiles;
    std::vector<fs::path> tmpFiles;

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
    fs::path workDir;
    flat_hash_set<fs::path> activeCommandFiles;
};
