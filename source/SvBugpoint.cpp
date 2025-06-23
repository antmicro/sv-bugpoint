// SPDX-License-Identifier: Apache-2.0
#include "SvBugpoint.hpp"
#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceManager.h>
#include <sys/wait.h>
#include <filesystem>
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
                 std::string passIdx,
                 SvBugpoint* svBugpoint) {
    bool committed = false;
    bool traversalDone = false;

    while (!traversalDone) {
        auto stats = AttemptStats(passIdx, stageName, svBugpoint);
        auto tmpTree = rewriter.transform(tree, traversalDone, stats);
        if (traversalDone && tmpTree == tree) {
            break;  // no change - no reason to test
        }
        if (svBugpoint->test(tmpTree, stats)) {
            tree = tmpTree;
            committed = true;
        }
    }
    return committed;
}

bool SvBugpoint::test(AttemptStats& stats) {
    // Execute ./sv-bugpoint-check.sh tmpFile.
    // On success (zero exit code) replace minimized file with tmp, and return true.
    // On fail (non-zero exit code) return false.
    stats.begin();
    pid_t pid = fork();
    if (pid == -1) {
        PRINTF_ERR("fork failed: %s\n", strerror(errno));
        exit(1);
    } else if (pid == 0) {  // we are inside child
        auto testArgs = getTestArgs();
        std::vector<std::string> argvString{};
        std::vector<char*> argv{};
        argvString.push_back(getCheckScript());
        for (auto& arg : testArgs) {
            argvString.push_back(arg);
        }
        for (auto& arg : argvString) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        if (execv(argv[0], argv.data())) {  // replace child with prog
            PRINTF_ERR("failed to launch '%s': %s\n", getCheckScript().c_str(), strerror(errno));
            kill(getppid(), SIGINT);  // terminate parent
            exit(1);
        }
    } else {  // we are in parent
        int wstatus;
        int rc = waitpid(pid, &wstatus, 0);
        if (rc <= 0 || !WIFEXITED(wstatus)) {
            perror("waitpid failed");
            exit(1);
        }
        if (WEXITSTATUS(wstatus)) {
            stats.end(false).report();
            return false;
        } else {
            stats.end(true).report();
            std::error_code ec;
            std::filesystem::copy(getTmpFile(), getMinimizedFile(),
                                  std::filesystem::copy_options::overwrite_existing, ec);
            saveCombinedOutput();
            if (ec) {
                std::cerr << "Error copying file: " << ec.message() << std::endl;
                exit(1);
            }
            return true;
        }
    }

    return false;  // just to make compiler happy - will never get here
}

bool SvBugpoint::test(std::shared_ptr<SyntaxTree>& tree, AttemptStats& stats) {
    // Write given tree to tmp file and execute ./sv-bugpoint-check.sh tmpFile.
    std::ofstream tmpFile;
    tmpFile.rdbuf()->pubsetbuf(
        0, 0);  // Enable unbuffered io. Has to be called before open to be effective
    tmpFile.open(getTmpFile());
    tmpFile << SyntaxPrinter::printFile(*tree);
    return test(stats);
}

bool SvBugpoint::pass(std::shared_ptr<SyntaxTree>& tree, const std::string& passIdx) {
    bool commited = false;

    commited |= rewriteLoop<BodyRemover>(tree, "bodyRemover", passIdx, this);
    commited |= rewriteLoop<InstantationRemover>(tree, "instantiationRemover", passIdx, this);
    commited |= rewriteLoop<BindRemover>(tree, "bindRemover", passIdx, this);
    commited |= rewriteLoop<BodyPartsRemover>(tree, "bodyPartsRemover", passIdx, this);
    commited |= rewriteLoop(makeExternRemover(tree), tree, "externRemover", passIdx, this);
    commited |= rewriteLoop<DeclRemover>(tree, "declRemover", passIdx, this);
    commited |= rewriteLoop<StatementsRemover>(tree, "statementsRemover", passIdx, this);
    commited |= rewriteLoop<ImportsRemover>(tree, "importsRemover", passIdx, this);
    commited |= rewriteLoop<ParamAssignRemover>(tree, "paramAssignRemover", passIdx, this);
    commited |= rewriteLoop<ContAssignRemover>(tree, "contAssignRemover", passIdx, this);
    commited |= rewriteLoop<MemberRemover>(tree, "memberRemover", passIdx, this);
    commited |= rewriteLoop<ModportRemover>(tree, "modportRemover", passIdx, this);
    commited |= rewriteLoop(makePortsRemover(tree), tree, "portsRemover", passIdx, this);
    commited |= rewriteLoop(makeStructFieldRemover(tree), tree, "structRemover", passIdx, this);
    commited |= rewriteLoop<ModuleRemover>(tree, "moduleRemover", passIdx, this);
    commited |= rewriteLoop<TypeSimplifier>(tree, "typeSimplifier", passIdx, this);

    return commited;
}

void SvBugpoint::minimize() {
    removeVerilatorConfig();
    bool anyChange = true;
    while (anyChange) {
        anyChange = false;
        // Create a new SourceManager per each loop as it caches the file content
        SourceManager sourceManager;
        BumpAllocator alloc;
        std::vector<slang::parsing::Trivia> newTrivia;
        for (size_t i = 0; i < minimizedFiles.size(); i++) {
            currentPathIdx = i;
            auto treeOrErr = SyntaxTree::fromFile(std::string(getMinimizedFile()), sourceManager);

            if (treeOrErr) {
                auto tree = *treeOrErr;
                // Comments are not separate SyntaxNodes, but trivia of SyntaxNodes
                // Make sure that we remove all comments from the first SyntaxNode
                // to match the behavior when we would remove first node.
                slang::parsing::Token* firstToken = tree->root().getFirstTokenPtr();
                const auto& trivia = firstToken->trivia();
                if (std::find_if(trivia.begin(), trivia.end(), [](const slang::parsing::Trivia& t) {
                        return t.kind == slang::parsing::TriviaKind::LineComment;
                    }) != trivia.end()) {
                    for (const auto& t : trivia) {
                        // Copy all trivia except line comments
                        if (t.kind != slang::parsing::TriviaKind::LineComment) {
                            newTrivia.push_back(t.clone(alloc));
                        } else {
                            // In case of comments, just create empty trivia so we would always have
                            // the same number of lines
                            newTrivia.push_back(slang::parsing::Trivia{
                                slang::parsing::TriviaKind::LineComment, ""});
                        }
                    }
                    *firstToken = firstToken->withTrivia(alloc, newTrivia);
                }

                int passIdx = 1;
                bool committed;
                do {
                    committed = pass(tree, std::to_string(passIdx++));
                    anyChange |= committed;
                } while (committed);

            } else {
                PRINTF_ERR("failed to load '%s' file: %s\n", getOriginalFile().c_str(),
                           std::string(treeOrErr.error().second).c_str());
                exit(1);
            }
        }
    }
}

void SvBugpoint::removeVerilatorConfig() {
    auto info = AttemptStats("-", "verilatorConfigRemover", this);
    for (size_t i = 0; i < minimizedFiles.size(); i++) {
        currentPathIdx = i;
        std::ifstream inputFile(getMinimizedFile());
        std::ofstream testFile(getTmpFile());
        std::string line;
        bool doSkip = false;
        bool skippedSomething = false;
        while (std::getline(inputFile, line)) {
            if (line == "`verilator_config") {
                doSkip = true;
                skippedSomething = true;
            } else if (doSkip && line.starts_with("`begin_keywords")) {
                doSkip = false;
                // There is chance that `begin_keywords is meant to do more than
                // merely exit configuration block, so we don't skip it.
                testFile << line << "\n";
            } else if (!doSkip) {
                testFile << line << "\n";
            }
        }
        testFile << std::flush;
        if (skippedSomething) {  // no reason to test if no modification was done
            test(info);
        }
    }
}

fs::path findCommonAncestor(fs::path a, fs::path b) {
    fs::path common;
    auto it_a = a.begin();
    auto it_b = b.begin();
    while (it_a != a.end() && it_b != b.end() && *it_a == *it_b) {
        common /= *it_a;
        it_a++;
        it_b++;
    }
    return common;
}

fs::path findCommonAncestor(std::vector<fs::path> paths) {
    fs::path commonAncestor = fs::canonical(paths[0]).parent_path();
    for (auto& path : paths) {
        commonAncestor = findCommonAncestor(commonAncestor, fs::canonical(path));
    }
    return commonAncestor;
}

void SvBugpoint::initOutDir() {
    // sort paths to have deterministic order
    std::sort(inputFiles.begin(), inputFiles.end());

    // recreate file structure in output directories
    fs::path commonAncestor = findCommonAncestor(inputFiles);
    for (auto& input : inputFiles) {
        fs::path relative = fs::relative(input, commonAncestor);
        minimizedFiles.push_back(getOutDir() / relative);
        tmpFiles.push_back(getTmpOutDir() / relative);
    }

    mkdir(getWorkDir());
    if (!std::filesystem::is_empty(getWorkDir()) && !force.value_or(false)) {
        std::cerr << getWorkDir() << " is not empty directory. Continue? [Y/n] ";
        int ch = std::cin.get();
        if (ch != '\n' && ch != 'Y' && ch != 'y') {
            exit(0);
        }
    }
    mkdir(getOutDir());
    mkdir(getTmpOutDir());
    mkdir(getDebugDir());
    mkdir(getTraceDir());
    if (saveIntermediates.value_or(false)) {
        mkdir(getIntermediateDir());
    }
    // NOTE: not removing old files may be misleading (especially having an intermediate dir)
    // Maybe add some kind of purge?
    for (size_t i = 0; i < inputFiles.size(); i++) {
        currentPathIdx = i;
        mkdir(getMinimizedFile().parent_path());
        mkdir(getTmpFile().parent_path());
        copyFile(getOriginalFile(), getMinimizedFile());
        copyFile(getOriginalFile(), getTmpFile());
        AttemptStats::writeHeader(getTraceFile());
    }
    saveCombinedOutput();
}

void SvBugpoint::dryRun() {
    auto info = AttemptStats("-", "dryRun", this);
    if (!test(info)) {
        PRINTF_ERR("'%s %s' exited with non-zero on dry run with unmodified input\n",
                   getCheckScript().c_str(), getTmpFile().c_str());
        exit(1);
    }
}

void SvBugpoint::checkDumpTrees() {
    if (dump.value_or(false)) {
        for (size_t i = 0; i < inputFiles.size(); i++) {
            currentPathIdx = i;
            auto treeOrErr = SyntaxTree::fromFile(std::string(getOriginalFile()));
            if (treeOrErr) {
                auto tree = *treeOrErr;

                std::ofstream syntaxDumpFile(getDumpSyntaxFile()), astDumpFile(getDumpAstFile());
                printSyntaxTree(tree, syntaxDumpFile);

                Compilation compilation;
                compilation.addSyntaxTree(tree);
                compilation.getAllDiagnostics();  // kludge for launching full elaboration

                printAst(compilation.getRoot(), astDumpFile);
            } else {
                PRINTF_ERR("failed to load '%s' file: %s\n", getOriginalFile().c_str(),
                           std::string(treeOrErr.error().second).c_str());
                exit(1);
            }
        }
    }
}

void SvBugpoint::saveCombinedOutput() {
    std::ofstream combinedOutput{getCombinedOutputFile()};
    int oldPathIdx = currentPathIdx;
    for (size_t i = 0; i < minimizedFiles.size(); i++) {
        currentPathIdx = i;
        auto minimalizedOutputPath = getMinimizedFile();
        // Don't append empty files
        if (!is_empty(minimalizedOutputPath) && file_size(minimalizedOutputPath) > 1) {
            std::ifstream input{getMinimizedFile()};
            combinedOutput << input.rdbuf();
        }
    }
    currentPathIdx = oldPathIdx;
}

void SvBugpoint::usage() {
    std::cerr << cmdLine.getHelpText("sv-bugpoint SystemVerilog minimalizer");
}

void SvBugpoint::addArgs() {
    cmdLine.add("-h,--help", showHelp, "Display available options");
    cmdLine.add("--force", force, "overwrite files in outDir without prompting");
    cmdLine.add("--save-intermediates", saveIntermediates, "save output of each removal attempt");
    cmdLine.add("--dump-trees", dump, "dump parse tree and elaborated AST of input code");
    cmdLine.add(
        "-f",
        [this](std::string_view value) {
            processCommandFiles(value);
            return "";
        },
        "One or more command files containing additional program options. "
        "Paths in the file are considered relative to the current directory.",
        "<file-pattern>[,...]", CommandLineFlags::CommaList);
    cmdLine.add(
        "-y",
        [this](std::string_view value) {
            addFilesFromDirectory(value);
            return "";
        },
        "Adds all files from directory", "<dir-pattern>[,...]", CommandLineFlags::CommaList);

    cmdLine.setPositional(
        [this](std::string_view value) {
            if (workDir.empty()) {
                workDir = value;
            } else if (checkScript.empty()) {
                checkScript = value;
            } else {
                addPath(value);
            }
            return "";
        },
        "outDir/ checkscript.sh input-files");
}

void SvBugpoint::addFilesFromDirectory(fs::path dir) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension();
            if (ext == ".sv" || ext == ".svh" || ext == ".v" || ext == ".vh") {
                addPath(entry.path().string());
            }
        }
    }
}

void SvBugpoint::processCommandFiles(fs::path file) {
    if (!activeCommandFiles.insert(file).second) {
        std::cerr << "command file '" << file << "' includes itself recursively" << std::endl;
        exit(1);
    }

    std::string inputStr;
    std::string line;
    std::ifstream input(file);
    while (std::getline(input, line)) {
        if (!line.starts_with("-")) {
            inputStr.append((file.parent_path() / fs::path(line)).string() + " ");
        }
    }

    CommandLine::ParseOptions parseOpts;
    parseOpts.expandEnvVars = true;
    parseOpts.ignoreProgramName = true;
    parseOpts.supportComments = true;
    parseOpts.ignoreDuplicates = true;
    parseCommandLine(inputStr, parseOpts);

    activeCommandFiles.erase(file);
}

void SvBugpoint::parseCommandLine(std::string_view argList,
                                  CommandLine::ParseOptions parseOptions) {
    if (!cmdLine.parse(argList, parseOptions)) {
        for (auto& err : cmdLine.getErrors())
            std::cerr << err << std::endl;
        exit(1);
    }
}

void SvBugpoint::parseCommandLine(int argc, char** argv) {
    if (!cmdLine.parse(argc, argv)) {
        for (auto& err : cmdLine.getErrors())
            std::cerr << err << std::endl;
        exit(1);
    }
}

void SvBugpoint::parseArgs(int argc, char** argv) {
    parseCommandLine(argc, argv);
    if (showHelp.value_or(false)) {
        usage();
        exit(0);
    }
    if (inputFiles.empty() || workDir.empty() || checkScript.empty()) {
        usage();
        exit(1);
    }
    if (!checkScript.starts_with("./")) {
        // check script is fed to execv that may need this (it is implementation-defined what
        // happens when there is no slash)
        checkScript = "./" + checkScript;
    }
}

int main(int argc, char** argv) {
    SvBugpoint svBugpoint = SvBugpoint();
    svBugpoint.addArgs();

    svBugpoint.parseArgs(argc, argv);

    svBugpoint.initOutDir();

    svBugpoint.checkDumpTrees();

    svBugpoint.dryRun();

    svBugpoint.minimize();

    svBugpoint.saveCombinedOutput();
}
