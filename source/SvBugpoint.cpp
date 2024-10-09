// SPDX-License-Identifier: Apache-2.0
#include <slang/syntax/SyntaxTree.h>
#include <iostream>
#include "OneTimeRewritersFwd.hpp"
#include "PairRemovers.hpp"
#include "Utils.hpp"

using namespace slang::syntax;
using namespace slang::ast;
using namespace slang;

bool rewriteLoop(PairRemover rewriter,
                std::shared_ptr<SyntaxTree>& tree,
                std::string stageName,
                std::string passIdx) {
    bool committed = false;
    bool traversalDone = false;

    while (!traversalDone) {
        auto stats = AttemptStats(passIdx, stageName);
        auto tmpTree = rewriter.transform(tree, traversalDone, stats);
        if (traversalDone && tmpTree == tree) {
            break;  // no change - no reason to test
        }
        if (test(tmpTree, stats)) {
            tree = tmpTree;
            committed = true;
        }
    }
    return committed;
}

bool pass(std::shared_ptr<SyntaxTree>& tree, const std::string& passIdx = "-") {
    bool commited = false;

    commited |= rewriteLoop<BodyRemover>(tree, "bodyRemover", passIdx);
    commited |= rewriteLoop<InstantationRemover>(tree, "instantiationRemover", passIdx);
    commited |= rewriteLoop<BodyPartsRemover>(tree, "bodyPartsRemover", passIdx);
    commited |= rewriteLoop(makeExternRemover(tree), tree, "externRemover", passIdx);
    commited |= rewriteLoop<DeclRemover>(tree, "declRemover", passIdx);
    commited |= rewriteLoop<StatementsRemover>(tree, "statementsRemover", passIdx);
    commited |= rewriteLoop<TypeSimplifier>(tree, "typeSimplifier", passIdx);
    commited |= rewriteLoop<ImportsRemover>(tree, "importsRemover", passIdx);
    commited |= rewriteLoop<ParamAssignRemover>(tree, "paramAssignRemover", passIdx);
    commited |= rewriteLoop<ContAssignRemover>(tree, "contAssignRemover", passIdx);
    commited |= rewriteLoop<MemberRemover>(tree, "memberRemover", passIdx);
    commited |= rewriteLoop<ModportRemover>(tree, "modportRemover", passIdx);
    commited |= rewriteLoop(makePortsRemover(tree), tree, "portsRemover", passIdx);
    commited |= rewriteLoop(makeStructFieldRemover(tree), tree, "structRemover", passIdx);

    return commited;
}

bool removeVerilatorConfig() {
    auto info = AttemptStats("-", "verilatorConfigRemover");
    std::ifstream inputFile(paths.input);
    std::ofstream testFile(paths.tmpOutput);
    std::string line;
    while (std::getline(inputFile, line) && line != "`verilator_config") {
        testFile << line << "\n";
    }
    testFile << std::flush;
    if (line == "`verilator_config")
        return test(info);
    else
        return false;
}

void minimize() {
    removeVerilatorConfig();

    auto treeOrErr = SyntaxTree::fromFile(paths.output);

    if (treeOrErr) {
        auto tree = *treeOrErr;

        int passIdx = 1;
        bool committed;
        do {
            committed = pass(tree, std::to_string(passIdx++));
        } while (committed);

    } else {
        std::cerr << "sv-bugpoint: failed to load " << paths.input << " file "
                  << treeOrErr.error().second << "\n";
        exit(1);
    }
}

void initOutDir(bool force) {
    mkdir(paths.outDir);
    if (!std::filesystem::is_empty(paths.outDir) && !force) {
        std::cerr << paths.outDir << " is not empty directory. Continue? [Y/n] ";
        int ch = std::cin.get();
        if (ch != '\n' && ch != 'Y' && ch != 'y') {
            exit(0);
        }
    }
    mkdir(paths.tmpDir);
    if (saveIntermediates)
        mkdir(paths.intermediateDir);
    // NOTE: not removing old files may be misleading (especially having an intermediate dir)
    // Maybe add some kind of purge?
    copyFile(paths.input, paths.output);
    copyFile(paths.input, paths.tmpOutput);
    AttemptStats::writeHeader();
}

void dryRun() {
    auto info = AttemptStats("-", "dryRun");
    if(!test(info)) {
        std::cerr << "sv-bugpoint: '" << paths.checkScript << " " << paths.tmpOutput << "' exited with non-zero on dry run with unmodified input.\n";
        exit(1);
    }
}

void usage() {
    std::cerr << "Usage: sv-bugpoint [options] outDir/ checkscript.sh input.sv\n";
    std::cerr << "Options:\n";
    std::cerr << " --force: overwrite files in outDir without prompting\n";
    std::cerr << " --save-intermediates: save output of each removal attempt\n";
    std::cerr << " --dump-trees: dump parse tree and elaborated AST of input code\n";
}

int main(int argc, char** argv) {
    bool dump = false;
    bool force = false;
    std::vector<std::string> positionalArgs;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            exit(0);
        } else if (strcmp(argv[i], "--force") == 0) {
            force = true;
        } else if (strcmp(argv[i], "--save-intermediates") == 0) {
            saveIntermediates = true;
        } else if (strcmp(argv[i], "--dump-trees") == 0) {
            dump = true;
        } else {
            positionalArgs.push_back(argv[i]);
        }
    }

    if (positionalArgs.size() != 3) {
        usage();
        exit(1);
    }

    paths = Paths(positionalArgs[0], positionalArgs[1], positionalArgs[2]);
    initOutDir(force);

    if (dump) {
        dumpTrees();
    }

    dryRun();

    minimize();
}
