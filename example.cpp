#include "slang/syntax/SyntaxNode.h"
#include "slang/util/BumpAllocator.h"
#include <chrono>
#include <slang/text/SourceManager.h>
#include <slang/text/SourceLocation.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include "debug.hpp"

using namespace slang::syntax;
using namespace slang;

const std::string originalFilename = "uvm.sv";
const std::string outputFilename = "uvm_minimized.sv";
const std::string tmpFilename = "uvm_test.sv";
const std::string statsFilename = "bugpoint_stats";

#define DERIVED static_cast<TDerived*>(this)

template<typename TDerived>
class OneTimeRemover: public SyntaxRewriter<TDerived> {
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

    template<typename T>
    void visit(T&& node) {
        if(state == SKIP_TO_START && node.sourceRange() == startPoint) {
            state = REMOVAL_ALLOWED;
        }

        if(state == REGISTER_CHILD && node.sourceRange() != SourceRange::NoLocation) {
          removedChild = node.sourceRange();
          state = WAIT_FOR_PARENT_EXIT;
          return;
        }

        if(state == REGISTER_SUCCESSOR && node.sourceRange() != SourceRange::NoLocation) {
          removedSuccessor = node.sourceRange();
          state = SKIP_TO_END;
          return;
        }

        if(state == SKIP_TO_END || state == WAIT_FOR_PARENT_EXIT) {
          return;
        }


        if constexpr (requires { DERIVED->handle(node); }) {
            DERIVED->handle(node);
        }
        else {
            DERIVED->visitDefault(node);
        }

        if((state == REGISTER_CHILD || state == WAIT_FOR_PARENT_EXIT) && node.sourceRange() == removed) {
          state = REGISTER_SUCCESSOR;
        }
  }

  template<typename T>
  void removeNode(const T& node) {
      if(state == REMOVAL_ALLOWED) {
        std::cerr << typeid(T).name() << "\n";
        std::cerr << node.toString() << "\n";
        DERIVED->remove(node);
        removed = node.sourceRange();
        state = REGISTER_CHILD;
      }
  }

  template<typename TParent, typename TChild>
  void removeChildList(const TParent& parent, const SyntaxList<TChild>& childList) {
      if(state == REMOVAL_ALLOWED && childList.getChildCount()) {
        std::cerr << typeid(TParent).name() << "\n";
        for(auto item: childList) {
          DERIVED->remove(*item);
          std::cerr << item->toString();
        }
        removed = parent.sourceRange();
        state = REGISTER_CHILD; // TODO: examine whether we register right child here
      }
  }

  std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree>& tree, bool& traversalDone) {
      // Apply one removal, and return changed tree.
      // traversalDone is set when subsequent calls to transform would not make sense
      removed = SourceRange::NoLocation;
      removedChild = SourceRange::NoLocation;
      removedSuccessor = SourceRange::NoLocation;

      auto tree2 = SyntaxRewriter<TDerived>::transform(tree);

      // I'm not sure about what intended behavior is, but head of SyntaxRewriter's allocator is nulled after traversal,
      // leading to NULL dereference when rewriter is reused. This is dirty work around this. TODO: examine it more carefully.
      this->alloc = BumpAllocator();

      if(removedChild == SourceRange::NoLocation && removedSuccessor == SourceRange::NoLocation) {
        traversalDone = true;
      }

      return tree2;
  }

  void moveStartToSuccesor() {
      // Start next transform from succesor of removed node.
      // Meant be run when you decided to commit removal (i.e you pass just transformed tree for next transform)
      startPoint = removedSuccessor;
      state = SKIP_TO_START;
  }

  void moveStartToChildOrSuccesor() {
      // Start next transform from child of removed node if possible, otherwise, from its succesor.
      // Meant to be run when you decided to rollback removal (i.e. you discard just transformed tree)
      if(removedChild != SourceRange::NoLocation) {
        startPoint = removedChild;
      } else {
        startPoint = removedSuccessor;
      }
      state = SKIP_TO_START;
  }
};

class BodyPartsRemover: public OneTimeRemover<BodyPartsRemover> {
  public:
  void handle(const LoopGenerateSyntax& node) {
    removeNode(node);
    visitDefault(node);
  }
  void handle(const ConcurrentAssertionMemberSyntax& node) {
    removeNode(node);
    visitDefault(node);
  }
};

class BodyRemover: public OneTimeRemover<BodyRemover> {
  public:
  void handle(const FunctionDeclarationSyntax& node) {
      removeChildList(node, node.items);
      visitDefault(node);
  }

  void handle(const ModuleDeclarationSyntax& node) {
      removeChildList(node, node.members);
      visitDefault(node);
  }
};

class DeclRemover: public OneTimeRemover<DeclRemover> {
  public:
  void handle(const FunctionDeclarationSyntax& node) {
      removeNode(node);
      visitDefault(node);
  }

  void handle(const ModuleDeclarationSyntax& node) {
      removeNode(node);
      visitDefault(node);
  }

  void handle(const TypedefDeclarationSyntax& node) {
      removeNode(node);
      visitDefault(node);
  }
};

class StatementsRemover: public OneTimeRemover<StatementsRemover> {
  public:
  void handle(const ProceduralBlockSyntax& node) {
      removeNode(node);
      visitDefault(node);
  }
  void handle(const CaseStatementSyntax& node) {
      removeNode(node);
      visitDefault(node);
  }
  void handle(const LoopStatementSyntax& node) {
      removeNode(node);
      visitDefault(node);
  }
};

class ImportsRemover: public OneTimeRemover<ImportsRemover> {
  public:
  void handle(const PackageImportDeclarationSyntax& node) {
    removeNode(node);
    visitDefault(node);
  }
  // void handle(const ModuleHeaderSyntax& node) {
  //     removeChildList(node, node.imports);
  // }
};

class MemberRemover: public OneTimeRemover<MemberRemover> {
  public:
  bool inEnum = false;

  void handle(const DataDeclarationSyntax& node) {
      removeNode(node);
  }

  void handle(const StructUnionMemberSyntax& node) {
      removeNode(node);
  }

  void handle(const EnumTypeSyntax& node) {
      inEnum = true;
      visitDefault(node);
      inEnum = false;
  }

  void handle(const DeclaratorSyntax& node) { // a.o. enum fields
      // DeclaratorSyntax is often wrapped in not_null
      // as temporary hack we only remove it from enum, when we know that it is not a case
      if(inEnum) {
        removeNode(node);
      }
  }

  void handle(const ParameterDeclarationStatementSyntax& node) {
      removeNode(node);
  }

  void handle(const ParameterDeclarationBaseSyntax& node) {
      removeNode(node);
  }
};

class ParamAssignRemover: public OneTimeRemover<ParamAssignRemover> {
  public:
  void handle(const ParameterValueAssignmentSyntax& node) {
      removeNode(node);
  }
};

class ContAssignRemover: public OneTimeRemover<ContAssignRemover> {
  public:
  void handle(const ContinuousAssignSyntax& node) {
      removeNode(node);
  }
};


class ModportRemover: public OneTimeRemover<ModportRemover> {
  public:
  void handle(const ModportDeclarationSyntax& node) {
      removeNode(node);
  }
};
class InstantationRemover: public OneTimeRemover<InstantationRemover> {
  public:
  void handle(const HierarchyInstantiationSyntax& node) {
      removeNode(node);
  }
};


bool test() {
  // Execute ./test.sh tmpFile.
  // On success (zero exit code) replace minimized file with tmp, and return true.
  // On fail (non-zero exit code) return false.

  pid_t pid = fork();
  if(pid == -1) {
      perror("fork failed");
      exit(1);
  }
  else if(pid == 0) { // we are inside child
      const char* const argv[] = {"./test.sh", tmpFilename.c_str(), NULL};
      if(execvp(argv[0], const_cast<char* const*>(argv))) { // replace child with prog
          perror("child: execvp error");
          _exit(1);
      }
  }
  else { // we are in parent
      int wstatus;
      int rc = waitpid(pid, &wstatus, 0);
      if(rc <= 0 || !WIFEXITED(wstatus)) {
        perror("waitpid failed");
        exit(1);
      }
      if(WEXITSTATUS(wstatus)) {
        return false;
      }
      else {
        std::filesystem::copy(tmpFilename, outputFilename, std::filesystem::copy_options::overwrite_existing);
        return true;
      }
  }

  return false; // just to make compiler happy - will never get here
}

bool test(std::shared_ptr<SyntaxTree>& tree) {
  // Write given tree to tmp file and execute ./test.sh tmpFile.
  std::ofstream tmpFile;
  tmpFile.rdbuf()->pubsetbuf(0, 0); // Enable unbuffered io. Has to be called before open to be effective
  tmpFile.open(tmpFilename);
  tmpFile << SyntaxPrinter::printFile(*tree);
  return test();
}

int countLines(std::string filename) {
    int count = 0;
    std::ifstream file(filename);
    for(std::string line; std::getline(file, line); ++count) {} // probably not a smartest way, but should be fine
    return count;
}

struct Stats {
  int commits = 0;
  int rollbacks = 0;
  int linesBefore;
  int linesAfter;
  std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
  std::chrono::time_point<std::chrono::high_resolution_clock> endTime;

  void begin()
  {
    linesBefore = countLines(outputFilename);
    startTime = std::chrono::high_resolution_clock::now();
  }

  void end()
  {
    linesAfter = countLines(outputFilename);
    endTime = std::chrono::high_resolution_clock::now();
  }

  std::string toStr(std::string pass, std::string stage)
  {
    std::stringstream tmp;
    int lines = linesBefore - linesAfter;
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    int attempts = rollbacks + commits;
    tmp << pass << '\t' << stage << '\t' << lines << '\t' << commits << '\t' << rollbacks << '\t' << attempts << '\t' << duration << '\n';
    return tmp.str();
  }

  void report(std::string pass, std::string stage)
  {
    std::cerr << toStr(pass, stage);
    std::ofstream file(statsFilename, std::ios_base::app);
    file << toStr(pass, stage);
  }

  static void writeHeader() {
    std::ofstream file(statsFilename);
    file << "pass\tstage\tlines_removed\tcommits\trollbacks\tattempts\ttime\n";
  }

  void addAttempts(Stats rhs) {
    commits += rhs.commits;
    rollbacks += rhs.rollbacks;
  }
};

template<typename T>
Stats removeLoop(OneTimeRemover<T> rewriter, std::shared_ptr<SyntaxTree>& tree, std::string stageName, std::string passIdx) {
  Stats stats;
  stats.begin();
  bool done = false;
  while(!done) {
    auto tmpTree = rewriter.transform(tree, done);
    if(test(tmpTree)) {
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

// Stats removeLoop2(std::shared_ptr<SyntaxTree>& tree) {
// }

Stats pass(std::shared_ptr<SyntaxTree>& tree, std::string passIdx="-") {
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
  auto treeOrErr = SyntaxTree::fromFile("uvm.sv");
  if (treeOrErr) {
      auto tree = *treeOrErr;
      AllPrinter printer(2);
      printer.visit(tree->root());
  } else {
      /* do something with result.error() */
  }
}

void removeVerilatorConfig() {
  std::ifstream inputFile(originalFilename);
  std::ofstream testFile(tmpFilename);
  std::string line;
  while(std::getline(inputFile, line) && line != "`verilator_config") {
    testFile << line << "\n";
  }
  testFile << std::flush;
  test();
}

void minimize() {
  std::filesystem::copy(originalFilename, outputFilename, std::filesystem::copy_options::overwrite_existing);
  removeVerilatorConfig();

  auto treeOrErr = SyntaxTree::fromFile(outputFilename);
  Stats::writeHeader();

  if (treeOrErr) {
      auto tree = *treeOrErr;

      Stats stats;
      stats.begin();

      int i = 1;
      Stats substats;
      do {
        substats = pass(tree, std::to_string(i++));
        stats.addAttempts(substats);
      } while(substats.linesAfter < substats.linesBefore);

      stats.end();
      stats.report("*","*");
  }
  else {
      /* do something with result.error() */
  }
}

int main() {
  // inspect();
  minimize();
}
