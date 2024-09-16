#include <slang/ast/ASTVisitor.h>
#include <slang/ast/Compilation.h>
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include "OneTimeRemoversFwd.hpp"
#include "PairRemovers.hpp"
#include "utils.hpp"

using namespace slang::syntax;
using namespace slang::ast;
using namespace slang;

bool removeLoop(PairRemover rewriter,
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

    commited |= removeLoop<BodyRemover>(tree, "bodyRemover", passIdx);
    commited |= removeLoop<InstantationRemover>(tree, "instantiationRemover", passIdx);
    commited |= removeLoop<BodyPartsRemover>(tree, "bodyPartsRemover", passIdx);
    commited |= removeLoop(makeExternRemover(tree), tree, "externRemover", passIdx);
    commited |= removeLoop<DeclRemover>(tree, "declRemover", passIdx);
    commited |= removeLoop<StatementsRemover>(tree, "statementsRemover", passIdx);
    commited |= removeLoop<ImportsRemover>(tree, "importsRemover", passIdx);
    commited |= removeLoop<ParamAssignRemover>(tree, "paramAssignRemover", passIdx);
    commited |= removeLoop<ContAssignRemover>(tree, "contAssignRemover", passIdx);
    commited |= removeLoop<MemberRemover>(tree, "memberRemover", passIdx);
    commited |= removeLoop<ModportRemover>(tree, "modportRemover", passIdx);
    commited |= removeLoop(makePortsRemover(tree), tree, "portsRemover", passIdx);
    commited |= removeLoop(makeStructFieldRemover(tree), tree, "structRemover", passIdx);

    return commited;
}

void dumpTrees() {
    auto treeOrErr = SyntaxTree::fromFile(paths.input);
    if (treeOrErr) {
        auto tree = *treeOrErr;

        std::ofstream syntaxDumpFile(paths.dumpSyntax), astDumpFile(paths.dumpAst);
        printSyntaxTree(tree, syntaxDumpFile);

        Compilation compilation;
        compilation.addSyntaxTree(tree);
        compilation.getAllDiagnostics();  // kludge for launching full elaboration

        printAst(compilation.getRoot(), astDumpFile);
    } else {
        std::cerr << "sv-bugpoint: failed to load " << paths.input << " file "
                  << treeOrErr.error().second << "\n";
        exit(1);
    }
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
    if (saveIntermediates)
        mkdir(paths.intermediateDir);
    // NOTE: not removing old files may be misleading (especially having an intermediate dir)
    // Maybe add some kind of purge?
    copyFile(paths.input, paths.output);
    AttemptStats::writeHeader();
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

    minimize();
}
