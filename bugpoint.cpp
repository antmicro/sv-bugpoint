#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "debug.hpp"
#include "slang/syntax/SyntaxNode.h"
#include "slang/util/BumpAllocator.h"

using namespace slang::syntax;
using namespace slang;

namespace files {
const std::string input = "./bugpoint_input.sv";
const std::string output = "./bugpoint_minimized.sv";
const std::string tmpOutput = "./bugpoint_tmp.sv";
const std::string checkScript = "./bugpoint_check.sh";
const std::string stats = "./bugpoint_stats";
}  // namespace files

#define DERIVED static_cast<TDerived*>(this)

template <typename TDerived>
class OneTimeRemover : public SyntaxRewriter<TDerived> {
  // Incremental node remover - each transform() yields one removal at most
 public:
  enum State {
    SKIP_TO_START,
    REMOVAL_ALLOWED,
    REGISTER_CHILD,
    WAIT_FOR_PARENT_EXIT,
    REGISTER_SUCCESSOR,
    SKIP_TO_END,
  };

  SourceRange startPoint;
  SourceRange removed;
  SourceRange removedChild;
  SourceRange removedSuccessor;

  State state = REMOVAL_ALLOWED;

  // As an optimization we do removal in quasi-sorted way:
  // first remove nodes at least 1024 lines long, then 512, and so on
  unsigned linesUpperLimit = INT_MAX;
  unsigned linesLowerLimit = 1024;

  /// The default handler invoked when no visit() method is overridden for a particular type.
  /// Will visit all child nodes by default.
  template <typename T>
  void visitDefault(T&& node) {
    for (uint32_t i = 0; i < node.getChildCount(); i++) {
      auto child = node.childNode(i);
      if (child)
        child->visit(*DERIVED, node.isChildOptional(i));
    }
  }

  template <typename T>
  void visit(T&& node, bool isNodeRemovable = true) {
    if (state == SKIP_TO_START && node.sourceRange() == startPoint) {
      state = REMOVAL_ALLOWED;
    }

    if (state == REGISTER_CHILD && node.sourceRange() != SourceRange::NoLocation) {
      removedChild = node.sourceRange();
      state = WAIT_FOR_PARENT_EXIT;
      return;
    }

    if (state == REGISTER_SUCCESSOR && node.sourceRange() != SourceRange::NoLocation) {
      removedSuccessor = node.sourceRange();
      state = SKIP_TO_END;
      return;
    }

    if (state == SKIP_TO_END || state == WAIT_FOR_PARENT_EXIT) {
      return;
    }

    if constexpr (requires { DERIVED->handle(node, isNodeRemovable); }) {
      DERIVED->handle(node, isNodeRemovable);
    } else {
      DERIVED->visitDefault(node);
    }

    if ((state == REGISTER_CHILD || state == WAIT_FOR_PARENT_EXIT) &&
        node.sourceRange() == removed) {
      state = REGISTER_SUCCESSOR;
    }
  }

  template <typename T>
  bool shouldRemove(const T& node, bool isNodeRemovable) {
    unsigned len = std::ranges::count(node.toString(), '\n') + 1;
    return state == REMOVAL_ALLOWED && isNodeRemovable && len >= linesLowerLimit &&
           len < linesUpperLimit;
  }

  template <typename T>
  bool shouldRemove(const SyntaxList<T>& list) {
    unsigned len = std::ranges::count(list.toString(), '\n') + 1;
    return state == REMOVAL_ALLOWED && list.getChildCount() && len >= linesLowerLimit &&
           len < linesUpperLimit;
  }

  template <typename T>
  void removeNode(const T& node, bool isNodeRemovable) {
    if (shouldRemove(node, isNodeRemovable)) {
      std::cerr << typeid(T).name() << "\n";
      std::cerr << node.toString() << "\n";
      DERIVED->remove(node);
      removed = node.sourceRange();
      state = REGISTER_CHILD;
    }
  }

  template <typename TParent, typename TChild>
  void removeChildList(const TParent& parent, const SyntaxList<TChild>& childList) {
    if (shouldRemove(childList)) {
      std::cerr << typeid(TParent).name() << "\n";
      for (auto item : childList) {
        DERIVED->remove(*item);
        std::cerr << item->toString();
      }
      removed = parent.sourceRange();
      state = REGISTER_CHILD;  // TODO: examine whether we register right child here
    }
  }

  std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree>& tree,
                                        bool& traversalDone) {
    // Apply one removal, and return changed tree.
    // traversalDone is set when subsequent calls to transform would not make sense
    removed = SourceRange::NoLocation;
    removedChild = SourceRange::NoLocation;
    removedSuccessor = SourceRange::NoLocation;

    auto tree2 = SyntaxRewriter<TDerived>::transform(tree);

    if (removedChild == SourceRange::NoLocation && removedSuccessor == SourceRange::NoLocation) {
      // we have ran out of nodes of searched size - advance limit
      linesUpperLimit = linesLowerLimit;
      linesLowerLimit /= 2;
      if (linesUpperLimit == 1) {  // tried all possible sizes - finish
        traversalDone = true;
      } else if (removed == SourceRange::NoLocation) {
        // no node removed - retry with new limit
        tree2 = transform(tree, traversalDone);
      }
    }

    return tree2;
  }

  void moveStartToSuccesor() {
    // Start next transform from succesor of removed node.
    // Meant be run when you decided to commit removal (i.e you pass just transformed tree for next
    // transform)
    startPoint = removedSuccessor;
    state = SKIP_TO_START;
  }

  void moveStartToChildOrSuccesor() {
    // Start next transform from child of removed node if possible, otherwise, from its succesor.
    // Meant to be run when you decided to rollback removal (i.e. you discard just transformed tree)
    if (removedChild != SourceRange::NoLocation) {
      startPoint = removedChild;
    } else {
      startPoint = removedSuccessor;
    }
    state = SKIP_TO_START;
  }
};

class BodyPartsRemover : public OneTimeRemover<BodyPartsRemover> {
 public:
  void handle(const LoopGenerateSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }
  void handle(const ConcurrentAssertionMemberSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }
};

class BodyRemover : public OneTimeRemover<BodyRemover> {
 public:
  void handle(const FunctionDeclarationSyntax& node, bool isNodeRemovable) {
    removeChildList(node, node.items);
    visitDefault(node);
  }

  void handle(const ModuleDeclarationSyntax& node, bool isNodeRemovable) {
    removeChildList(node, node.members);
    visitDefault(node);
  }
};

class DeclRemover : public OneTimeRemover<DeclRemover> {
 public:
  void handle(const FunctionDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }

  void handle(const ModuleDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }

  void handle(const TypedefDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }
};

class StatementsRemover : public OneTimeRemover<StatementsRemover> {
 public:
  void handle(const ProceduralBlockSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }
  void handle(const StatementSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }
};

class ImportsRemover : public OneTimeRemover<ImportsRemover> {
 public:
  void handle(const PackageImportDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    visitDefault(node);
  }
};

class MemberRemover : public OneTimeRemover<MemberRemover> {
 public:
  void handle(const DataDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }

  void handle(const StructUnionMemberSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }

  void handle(const DeclaratorSyntax& node, bool isNodeRemovable) {  // a.o. enum fields
    removeNode(node, isNodeRemovable);
  }

  void handle(const ParameterDeclarationStatementSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }

  void handle(const ParameterDeclarationBaseSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }
};

class ParamAssignRemover : public OneTimeRemover<ParamAssignRemover> {
 public:
  void handle(const ParameterValueAssignmentSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }
};

class ContAssignRemover : public OneTimeRemover<ContAssignRemover> {
 public:
  void handle(const ContinuousAssignSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }
};

class ModportRemover : public OneTimeRemover<ModportRemover> {
 public:
  void handle(const ModportDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }
};
class InstantationRemover : public OneTimeRemover<InstantationRemover> {
 public:
  void handle(const HierarchyInstantiationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
  }
};

bool test() {
  // Execute ./test.sh tmpFile.
  // On success (zero exit code) replace minimized file with tmp, and return true.
  // On fail (non-zero exit code) return false.

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    exit(1);
  } else if (pid == 0) {  // we are inside child
    const char* const argv[] = {files::checkScript.c_str(), files::tmpOutput.c_str(), NULL};
    if (execvp(argv[0], const_cast<char* const*>(argv))) {  // replace child with prog
      perror("child: execvp error");
      _exit(1);
    }
  } else {  // we are in parent
    int wstatus;
    int rc = waitpid(pid, &wstatus, 0);
    if (rc <= 0 || !WIFEXITED(wstatus)) {
      perror("waitpid failed");
      exit(1);
    }
    if (WEXITSTATUS(wstatus)) {
      return false;
    } else {
      std::filesystem::copy(files::tmpOutput, files::output,
                            std::filesystem::copy_options::overwrite_existing);
      return true;
    }
  }

  return false;  // just to make compiler happy - will never get here
}

bool test(std::shared_ptr<SyntaxTree>& tree) {
  // Write given tree to tmp file and execute ./test.sh tmpFile.
  std::ofstream tmpFile;
  tmpFile.rdbuf()->pubsetbuf(
      0, 0);  // Enable unbuffered io. Has to be called before open to be effective
  tmpFile.open(files::tmpOutput);
  tmpFile << SyntaxPrinter::printFile(*tree);
  return test();
}

int countLines(std::string filename) {
  int count = 0;
  std::ifstream file(filename);
  for (std::string line; std::getline(file, line); ++count) {
  }  // probably not a smartest way, but should be fine
  return count;
}

struct Stats {
  int commits = 0;
  int rollbacks = 0;
  int linesBefore;
  int linesAfter;
  std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
  std::chrono::time_point<std::chrono::high_resolution_clock> endTime;

  void begin() {
    linesBefore = countLines(files::output);
    startTime = std::chrono::high_resolution_clock::now();
  }

  void end() {
    linesAfter = countLines(files::output);
    endTime = std::chrono::high_resolution_clock::now();
  }

  std::string toStr(std::string pass, std::string stage) {
    std::stringstream tmp;
    int lines = linesBefore - linesAfter;
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    int attempts = rollbacks + commits;
    tmp << pass << '\t' << stage << '\t' << lines << '\t' << commits << '\t' << rollbacks << '\t'
        << attempts << '\t' << duration << '\n';
    return tmp.str();
  }

  void report(std::string pass, std::string stage) {
    std::cerr << toStr(pass, stage);
    std::ofstream file(files::stats, std::ios_base::app);
    file << toStr(pass, stage) << std::flush;
  }

  static void writeHeader() {
    std::ofstream file(files::stats);
    file << "pass\tstage\tlines_removed\tcommits\trollbacks\tattempts\ttime\n";
  }

  void addAttempts(Stats rhs) {
    commits += rhs.commits;
    rollbacks += rhs.rollbacks;
  }
};

template <typename T>
Stats removeLoop(OneTimeRemover<T> rewriter,
                 std::shared_ptr<SyntaxTree>& tree,
                 std::string stageName,
                 std::string passIdx) {
  Stats stats;
  stats.begin();
  bool traversalDone = false;
  while (!traversalDone) {
    auto tmpTree = rewriter.transform(tree, traversalDone);
    if (traversalDone && tmpTree == tree) {
      break;  // no change - no reason to test
    }
    if (test(tmpTree)) {
      tree = tmpTree;
      rewriter.moveStartToSuccesor();
      stats.commits++;
    } else {
      rewriter.moveStartToChildOrSuccesor();
      stats.rollbacks++;
    }
  }
  stats.end();
  stats.report(passIdx, stageName);
  return stats;
}

Stats pass(std::shared_ptr<SyntaxTree>& tree, std::string passIdx = "-") {
  Stats stats;
  stats.begin();

  stats.addAttempts(removeLoop(BodyRemover(), tree, "bodyRemover", passIdx));
  stats.addAttempts(removeLoop(InstantationRemover(), tree, "instantationRemover", passIdx));
  stats.addAttempts(removeLoop(BodyPartsRemover(), tree, "bodyPartsRemover", passIdx));
  stats.addAttempts(removeLoop(DeclRemover(), tree, "declRemover", passIdx));
  stats.addAttempts(removeLoop(StatementsRemover(), tree, "statementsRemover", passIdx));
  stats.addAttempts(removeLoop(ImportsRemover(), tree, "importsRemover", passIdx));
  stats.addAttempts(removeLoop(ParamAssignRemover(), tree, "paramAssignRemover", passIdx));
  stats.addAttempts(removeLoop(ContAssignRemover(), tree, "contAssignRemover", passIdx));
  stats.addAttempts(removeLoop(MemberRemover(), tree, "memberRemover", passIdx));
  stats.addAttempts(removeLoop(ModportRemover(), tree, "modportRemover", passIdx));

  stats.end();
  stats.report(passIdx, "*");

  return stats;
}

void inspect() {
  auto treeOrErr = SyntaxTree::fromFile(files::input);
  if (treeOrErr) {
    auto tree = *treeOrErr;
    AllPrinter printer(2);
    printer.visit(tree->root());
  } else {
    /* do something with result.error() */
  }
}

Stats removeVerilatorConfig() {
  Stats stats;
  stats.begin();
  std::ifstream inputFile(files::input);
  std::ofstream testFile(files::tmpOutput);
  std::string line;
  while (std::getline(inputFile, line) && line != "`verilator_config") {
    testFile << line << "\n";
  }
  testFile << std::flush;
  if (test()) {
    stats.commits++;
  } else {
    stats.rollbacks++;
  }
  stats.end();
  stats.report("-", "verilatorConfigRemover");
  return stats;
}

void minimize() {
  std::filesystem::copy(files::input, files::output,
                        std::filesystem::copy_options::overwrite_existing);
  Stats::writeHeader();
  Stats stats;
  stats.begin();
  stats.addAttempts(removeVerilatorConfig());

  auto treeOrErr = SyntaxTree::fromFile(files::output);

  if (treeOrErr) {
    auto tree = *treeOrErr;

    int i = 1;
    Stats substats;
    do {
      substats = pass(tree, std::to_string(i++));
      stats.addAttempts(substats);
    } while (substats.linesAfter < substats.linesBefore);

  } else {
    /* do something with result.error() */
  }
  stats.end();
  stats.report("*", "*");
}

int main() {
  // inspect();
  minimize();
}
