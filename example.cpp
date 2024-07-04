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

enum RewriterState {
  SKIP_TO_START,
  REMOVAL_ALLOWED,
  REGISTER_CHILD,
  WAIT_FOR_PARENT_EXIT,
  REGISTER_SUCCESSOR,
  SKIP_TO_END,
};

const std::string originalFilename = "uvm.sv";
const std::string outputFilename = "uvm_minimized.sv";
const std::string tmpFilename = "uvm_test.sv";
const std::string statsFilename = "bugpoint_stats";

#define DERIVED static_cast<TDerived*>(this)

template<typename TDerived>
class OneTimeRewriter: public slang::syntax::SyntaxRewriter<TDerived> {
  public:
    slang::SourceRange startPoint;
    slang::SourceRange removed;
    slang::SourceRange removedChild;
    slang::SourceRange removedSuccessor;

    RewriterState state = REMOVAL_ALLOWED;

    template<typename T>
    void visit(T&& t) {
        if(state == SKIP_TO_START && t.sourceRange() == startPoint) {
            state = REMOVAL_ALLOWED;
        }

        if(state == REGISTER_CHILD && t.sourceRange() != slang::SourceRange::NoLocation) {
          removedChild = t.sourceRange();
          state = WAIT_FOR_PARENT_EXIT;
          return;
        }

        if(state == REGISTER_SUCCESSOR && t.sourceRange() != slang::SourceRange::NoLocation) {
          removedSuccessor = t.sourceRange();
          state = SKIP_TO_END;
          return;
        }

        if(state == SKIP_TO_END || state == WAIT_FOR_PARENT_EXIT) {
          return;
        }


        if constexpr (requires { DERIVED->handle(t); }) {
            DERIVED->handle(t);
        }
        else {
            DERIVED->visitDefault(t);
        }

        if((state == REGISTER_CHILD || state == WAIT_FOR_PARENT_EXIT) && t.sourceRange() == removed) {
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
      DERIVED->visitDefault(node);
  }

  template<typename TParent, typename TChild>
  void removeChildList(const TParent& parent, const slang::syntax::SyntaxList<TChild>& childList) {
      if(state == REMOVAL_ALLOWED) {
        std::cerr << typeid(TParent).name() << "\n";
        for(auto item: childList) {
          DERIVED->remove(*item);
          std::cerr << item->toString();
        }
        removed = parent.sourceRange();
        state = REGISTER_CHILD; // TODO: examine whether we register right child here
      }
      DERIVED->visitDefault(parent);
  }

  std::shared_ptr<slang::syntax::SyntaxTree> transform(const std::shared_ptr<slang::syntax::SyntaxTree>& tree) {
      removed = slang::SourceRange::NoLocation;
      removedChild = slang::SourceRange::NoLocation;
      removedSuccessor = slang::SourceRange::NoLocation;

      auto tree2 = slang::syntax::SyntaxRewriter<TDerived>::transform(tree);

      // I'm not sure about what intended behavior is, but head of SyntaxRewriter's allocator is nulled after traversal,
      // leading to NULL dereference when rewriter is reused. This is dirty work around this. TODO: examine it more carefully.
      this->alloc = slang::BumpAllocator();

      if(state != SKIP_TO_END) { // it means that we traversed whole thing
        return nullptr;
      } else {
        return tree2;
      }
  }

  void moveToSuccesor() {
      startPoint = removedSuccessor;
      state = SKIP_TO_START;
  }

  void moveToChildOrSuccesor() {
      if(removedChild != slang::SourceRange::NoLocation) {
        startPoint = removedChild;
      } else {
        startPoint = removedSuccessor;
      }
      state = SKIP_TO_START;
  }
};

class GenforRemover: public OneTimeRewriter<GenforRemover> {
  public:
  void handle(const slang::syntax::LoopGenerateSyntax& node) {
    removeNode(node);
  }
};

class BodyRemover: public OneTimeRewriter<BodyRemover> {
  public:
  void handle(const slang::syntax::FunctionDeclarationSyntax& node) {
      removeChildList(node, node.items);
  }

  void handle(const slang::syntax::ModuleDeclarationSyntax& node) {
      removeChildList(node, node.members);
  }
};

class DeclRemover: public OneTimeRewriter<DeclRemover> {
  public:
  void handle(const slang::syntax::FunctionDeclarationSyntax& node) {
      removeNode(node);
  }

  void handle(const slang::syntax::ModuleDeclarationSyntax& node) {
      removeNode(node);
  }
};

// class AssertionPrinter: public slang::syntax::SyntaxVisitor<AssertionPrinter> {
//   public:
//   void handle(const slang::syntax::ActionBlockSyntax& node) {
//       std::cout << typeid(node).name() << "\n";
//       std::cout << node.toString() << "\n";
//       visitDefault(node);
//   }
// };

// class ActionBlockPrinter: public slang::syntax::SyntaxVisitor<ActionBlockPrinter> {
//   public:
//   void handle(const slang::syntax::ActionBlockSyntax& node) {
//       std::cout << node.toString() << "\n";
//       visitDefault(node);
//   }
// };

// class AllPrinter: public slang::syntax::SyntaxVisitor<AllPrinter> {
//   public:
//   template<typename T>
//   void handle(const T& node) {
//       std::cout << typeid(node).name() << "\n";
//       std::cout << node.toString() << "\n";
//       visitDefault(node);
//   }
// };

bool test(std::shared_ptr<slang::syntax::SyntaxTree>& tree) {
  std::ofstream file(tmpFilename);
  file.rdbuf()->pubsetbuf(0, 0);
  file << slang::syntax::SyntaxPrinter::printFile(*tree);

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

int countLines(std::string filename) {
    int count = 0;
    std::ifstream file(filename);
    for(std::string line; std::getline(file, line); ++count) {} // probably not a smartest way, but should be fine
    return count;
}

struct Stats {
  int successCount = 0;
  int rollbackCount = 0;
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

  std::string toStr()
  {
    std::stringstream tmp;
    int lines = linesBefore - linesAfter;
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    tmp << "lines removed: " << lines
        << ", successes: " << successCount
        << ", rollbacks: " << rollbackCount
        << ", seconds: " << duration
        <<  "\n";
    return tmp.str();
  }

  void report(std::string stageName)
  {
    std::cerr << stageName << " STAGE COMPLETE\n" << toStr();
    std::ofstream file(statsFilename, std::ios_base::app);
    file << stageName << " STAGE COMPLETE\n" << toStr();
  }
  // TODO:
  // accumulate(Stats rhs) // extend previous stat with sub-stat
};

template<typename T>
void removeLoop(OneTimeRewriter<T> rewriter, std::shared_ptr<slang::syntax::SyntaxTree>& tree, std::string stageName) {
  Stats stats;
  stats.begin();
  while(auto tmpTree = rewriter.transform(tree)) {
    if(test(tmpTree)) {
      tree = tmpTree;
      rewriter.moveToSuccesor();
      stats.successCount++;
    } else {
      rewriter.moveToChildOrSuccesor();
      stats.rollbackCount++;
    }
  }
  stats.end();
  stats.report(stageName);
}

int main() {
  auto treeOrErr = slang::syntax::SyntaxTree::fromFile(originalFilename);

  std::filesystem::copy(originalFilename, outputFilename, std::filesystem::copy_options::overwrite_existing);
  std::filesystem::remove(statsFilename);

  if (treeOrErr) {
      auto tree = *treeOrErr;
      // AllPrinter printer;
      // printer.visit(tree->root());
      removeLoop(GenforRemover(), tree, "genforRemover");
      removeLoop(BodyRemover(), tree, "bodyRemover");
      removeLoop(DeclRemover(), tree, "declRemover");
  }
  else {
      /* do something with result.error() */
  }
}
