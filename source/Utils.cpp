#include "Utils.hpp"
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __GLIBCXX__
#include <cxxabi.h>
std::string tryDemangle(const char* mangled) {
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

std::string toString(SourceRange sourceRange) {
    if (sourceRange == SourceRange::NoLocation)
        return "NO_LOCATION";
    else
        return "buffer:" + std::to_string(sourceRange.start().buffer().getId()) +
               ", offsetStart: " + std::to_string(sourceRange.start().offset()) +
               ", offsetEnd: " + std::to_string(sourceRange.end().offset());
}

Paths paths;
// Flag for saving intermediate output of each attempt
bool saveIntermediates = false;
// Global counter incremented after end of each attempt
// Meant mainly for setting up conditional breakpoints based on trace
int currentAttemptIdx = 0;

void copyFile(const std::string& from, const std::string& to) {
    try {
        std::filesystem::copy(from, to, std::filesystem::copy_options::overwrite_existing);
    } catch (const std::filesystem::filesystem_error& err) {
        std::cerr << "sv-bugpoint: failed to copy " << from << "to" << to << ": "
                  << err.code().message() << "\n";
        exit(1);
    }
}

void mkdir(const std::string& path) {
    try {
        std::filesystem::create_directory(path);
    } catch (const std::filesystem::filesystem_error& err) {
        std::cerr << "sv-bugpoint: failed to make directory " << path << ": "
                  << err.code().message() << "\n";
        exit(1);
    }
}

int countLines(const std::string& filename) {
    std::ifstream file(filename);
    return std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
}

AttemptStats& AttemptStats::begin() {
    linesBefore = countLines(paths.output);
    startTime = std::chrono::high_resolution_clock::now();
    idx = currentAttemptIdx;
    return *this;
}
AttemptStats& AttemptStats::end(bool committed) {
    this->committed = committed;
    linesAfter = countLines(paths.output);
    endTime = std::chrono::high_resolution_clock::now();
    if (saveIntermediates) {
        copyFile(paths.tmpOutput,
                 paths.intermediateDir + "/attempt" + std::to_string(currentAttemptIdx) + ".sv");
    }
    currentAttemptIdx++;
    return *this;
}

std::string AttemptStats::toStr() const {
    std::stringstream tmp;
    int lines = linesBefore - linesAfter;
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    tmp << pass << '\t' << stage << '\t' << lines << '\t' << committed << '\t' << duration << "ms\t"
        << idx << '\t' << typeInfo << "\n";
    return tmp.str();
}

void AttemptStats::report() {
    std::cerr << toStr();
    std::ofstream file(paths.trace, std::ios_base::app);
    file << toStr() << std::flush;
}

void AttemptStats::writeHeader() {
    std::ofstream file(paths.trace);
    file << "pass\tstage\tlines_removed\tcommitted\ttime\tidx\ttype_info\n";
}

bool test(AttemptStats& stats) {
    // Execute ./sv-bugpoint-check.sh tmpFile.
    // On success (zero exit code) replace minimized file with tmp, and return true.
    // On fail (non-zero exit code) return false.
    stats.begin();
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {  // we are inside child
        const char* const argv[] = {paths.checkScript.c_str(), paths.tmpOutput.c_str(), NULL};
        if (execv(argv[0], const_cast<char* const*>(argv))) {  // replace child with prog
            std::string err = "sv-bugpoint: failed to lanuch " + paths.checkScript;
            perror(err.c_str());
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
            std::filesystem::copy(paths.tmpOutput, paths.output,
                                  std::filesystem::copy_options::overwrite_existing);
            stats.end(true).report();
            return true;
        }
    }

    return false;  // just to make compiler happy - will never get here
}

bool test(std::shared_ptr<SyntaxTree>& tree, AttemptStats& info) {
    // Write given tree to tmp file and execute ./sv-bugpoint-check.sh tmpFile.
    std::ofstream tmpFile;
    tmpFile.rdbuf()->pubsetbuf(
        0, 0);  // Enable unbuffered io. Has to be called before open to be effective
    tmpFile.open(paths.tmpOutput);
    tmpFile << SyntaxPrinter::printFile(*tree);
    return test(info);
}
