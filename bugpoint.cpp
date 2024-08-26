#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/ast/Compilation.h>
#include <slang/ast/ASTVisitor.h>
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

using namespace slang::syntax;
using namespace slang::ast;
using namespace slang;

namespace files {
const std::string input = "./bugpoint_input.sv";
const std::string output = "./bugpoint_minimized.sv";
const std::string tmpOutput = "./bugpoint_tmp.sv";
const std::string checkScript = "./bugpoint_check.sh";
const std::string trace = "./bugpoint_trace";
}  // namespace files

int countLines(std::string filename) {
  int count = 0;
  std::ifstream file(filename);
  for (std::string line; std::getline(file, line); ++count) {
  }  // probably not a smartest way, but should be fine
  return count;
}

// Global counter incremented after end of each attempt
// Meant mainly for setting up conditional breakpoints based on trace
int currentAttemptIdx = 0;

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

  AttemptStats(std::string pass, std::string stage): pass(pass), stage(stage), committed(false)
  {}

  AttemptStats& begin() {
    linesBefore = countLines(files::output);
    startTime = std::chrono::high_resolution_clock::now();
    idx = currentAttemptIdx;
    return *this;
  }

  AttemptStats& end(bool committed) {
    this->committed = committed;
    linesAfter = countLines(files::output);
    endTime = std::chrono::high_resolution_clock::now();
    currentAttemptIdx++;
    return *this;
  }

  std::string toStr() const {
    std::stringstream tmp;
    int lines = linesBefore - linesAfter;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    tmp << pass << '\t' << stage << '\t' << lines << '\t' << committed << '\t' << duration << '\t' << idx << '\t' << typeInfo << "\n";
    return tmp.str();
  }

  void report() {
    std::cerr << toStr();
    std::ofstream file(files::trace, std::ios_base::app);
    file << toStr() << std::flush;
  }
  static void writeHeader() {
    std::ofstream file(files::trace);
    file << "pass\tstage\tlines_removed\tcommitted\ttime\tidx\ttype_info\n";
  }
};

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

  enum ShouldVisitChildren {
    VISIT_CHILDREN,
    DONT_VISIT_CHILDREN,
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

  std::string removedTypeInfo;

  /// Visit all child nodes
  template <typename T>
  void visitDefault(T&& node) {
    for (uint32_t i = 0; i < node.getChildCount(); i++) {
      auto child = node.childNode(i);
      if (child)
        child->visit(*DERIVED, node.isChildOptional(i));
    }
  }

  // Do some state bookkeeping, try to call type-specific handler, and visit node's children.
  // Visiting children can be disabled by returning DONT_VISIT_CHILDREN from handle()
  template <typename T>
  void visit(T&& node, bool isNodeRemovable = true) {
    if (state == SKIP_TO_START && node.sourceRange() == startPoint) {
      state = REMOVAL_ALLOWED;
    }

    if (state == REGISTER_CHILD && node.sourceRange() != SourceRange::NoLocation
        && node.sourceRange() != removed) { // avoid marking removed node as its own children
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
      if(DERIVED->handle(node, isNodeRemovable) == VISIT_CHILDREN) {
        DERIVED->visitDefault(node);
      }
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
  void logType() {
      std::cerr << STRINGIZE_NODE_TYPE(T) << "\n";
      removedTypeInfo = STRINGIZE_NODE_TYPE(T);
  }

  template <typename T>
  void removeNode(const T& node, bool isNodeRemovable) {
    if (shouldRemove(node, isNodeRemovable)) {
      logType<T>();
      std::cerr << node.toString() << "\n";
      DERIVED->remove(node);
      removed = node.sourceRange();
      state = REGISTER_CHILD;
    }
  }

  template <typename TParent, typename TChild>
  void removeChildList(const TParent& parent, const SyntaxList<TChild>& childList) {
    if (shouldRemove(childList)) {
      logType<TParent>();
      for (auto item : childList) {
        DERIVED->remove(*item);
        std::cerr << item->toString();
      }
      std::cerr << "\n";
      removed = parent.sourceRange();
      state = REGISTER_CHILD;  // TODO: examine whether we register right child here
    }
  }

  std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree>& tree,
                                        bool& traversalDone, AttemptStats& stats) {
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
        tree2 = transform(tree, traversalDone, stats);
      }
    }

    stats.typeInfo = removedTypeInfo;
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
  ShouldVisitChildren handle(const LoopGenerateSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
  ShouldVisitChildren handle(const ConcurrentAssertionMemberSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
  ShouldVisitChildren handle(const ElseClauseSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
};

class BodyRemover : public OneTimeRemover<BodyRemover> {
 public:
  ShouldVisitChildren handle(const ClassDeclarationSyntax& node, bool isNodeRemovable) {
    removeChildList(node, node.items);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const FunctionDeclarationSyntax& node, bool isNodeRemovable) {
    removeChildList(node, node.items);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ModuleDeclarationSyntax& node, bool isNodeRemovable) {
    removeChildList(node, node.members);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const BlockStatementSyntax& node, bool isNodeRemovable) {
    removeChildList(node, node.items);
    return VISIT_CHILDREN;
  }
};

class DeclRemover : public OneTimeRemover<DeclRemover> {
 public:
  ShouldVisitChildren handle(const FunctionDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ModuleDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const TypedefDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ForwardTypedefDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ClassDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ImplementsClauseSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ExtendsClauseSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ConstraintDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ClassMethodPrototypeSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ClassMethodDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
};

class StatementsRemover : public OneTimeRemover<StatementsRemover> {
 public:
  ShouldVisitChildren handle(const ProceduralBlockSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
  ShouldVisitChildren handle(const StatementSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
  ShouldVisitChildren handle(const LocalVariableDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
};

class ImportsRemover : public OneTimeRemover<ImportsRemover> {
 public:
  ShouldVisitChildren handle(const PackageImportDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return VISIT_CHILDREN;
  }
};

class MemberRemover : public OneTimeRemover<MemberRemover> {
 public:
  ShouldVisitChildren handle(const DataDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const StructUnionMemberSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const DeclaratorSyntax& node, bool isNodeRemovable) {  // a.o. enum fields
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ParameterDeclarationStatementSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ClassPropertyDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }

  ShouldVisitChildren handle(const ParameterDeclarationBaseSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }
};

class ParamAssignRemover : public OneTimeRemover<ParamAssignRemover> {
 public:
  ShouldVisitChildren handle(const ParameterValueAssignmentSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }
};

class ContAssignRemover : public OneTimeRemover<ContAssignRemover> {
 public:
  ShouldVisitChildren handle(const ContinuousAssignSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }
};

class ModportRemover : public OneTimeRemover<ModportRemover> {
 public:
  ShouldVisitChildren handle(const ModportDeclarationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }
};
class InstantationRemover : public OneTimeRemover<InstantationRemover> {
 public:
  ShouldVisitChildren handle(const HierarchyInstantiationSyntax& node, bool isNodeRemovable) {
    removeNode(node, isNodeRemovable);
    return DONT_VISIT_CHILDREN;
  }
};

class PairRemover : public SyntaxRewriter<PairRemover> {
  // each tranform yields removal of pair of nodes (based on locations in suplied pairs list)
 public:
    std::vector<std::pair<SourceRange, SourceRange>> pairs;
    std::pair<SourceRange, SourceRange> searchedPair;
    std::string removedTypeInfo;

    PairRemover(std::vector<std::pair<SourceRange, SourceRange>>&& pairs): pairs(pairs) {}

    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree>& tree,
                                          bool& traversalDone, AttemptStats& stats) {
      if(pairs.empty()) {
        traversalDone = true;
        return tree;
      }
      searchedPair = pairs.back();
      pairs.pop_back();
      removedTypeInfo = "";
      auto tree2 = SyntaxRewriter<PairRemover>::transform(tree);
      stats.typeInfo = removedTypeInfo;
      // maybe do it in while(not changed) loop
      traversalDone = pairs.empty();
      return tree2;
    }

  /// The default handler invoked when no visit() method is overridden for a particular type.
  /// Will visit all child nodes by default.
  template <typename T>
  void visitDefault(T&& node) {
    for (uint32_t i = 0; i < node.getChildCount(); i++) {
      auto child = node.childNode(i);
      if (child)
        child->visit(*this, node.isChildOptional(i));
    }
  }

  template <typename T>
  void logType() {
      std::cerr << STRINGIZE_NODE_TYPE(T) << "\n";
      removedTypeInfo += (removedTypeInfo.empty() ? "" : ",") + STRINGIZE_NODE_TYPE(T);
  }

  template <typename T>
  void visit(T&& node, bool isNodeRemovable = true) {
      bool found = node.sourceRange() == searchedPair.first || node.sourceRange() == searchedPair.second;
      if(isNodeRemovable && found && node.sourceRange() != SourceRange::NoLocation) {
        logType<T>();
        std::cerr << node.toString() << "\n";
        remove(node);
      }

      visitDefault(node);
  }
};

class ExternMapper : public ASTVisitor<ExternMapper, true, true, true> {
  // build vector of extern methods' pairs (prototypeLocation, implementationLocation)
  public:
    std::vector<std::pair<SourceRange, SourceRange>> pairs;

    void handle(const GenericClassDefSymbol& t) {
        ASTVisitor<ExternMapper, true, true, true>::visitDefault(t);
        if (t.numSpecializations() == 0) {
            // in order to visit members of not specialized class we create an artificial specialization
            t.getInvalidSpecialization().visit(*this);
        }
    }

    void handle(const MethodPrototypeSymbol& proto) {
      SourceRange protoLocation = SourceRange::NoLocation;
      SourceRange implLocation = SourceRange::NoLocation;
      if(proto.getSyntax()) {
        protoLocation = proto.getSyntax()->sourceRange();
      } else {
        exit(1);
      }

      auto impl = proto.getSubroutine();
      if(impl && impl->getSyntax()) {
          implLocation = impl->getSyntax()->sourceRange();
      }

      if(protoLocation != SourceRange::NoLocation || implLocation != SourceRange::NoLocation) {
        pairs.push_back({protoLocation, implLocation});
      }
      visitDefault(proto);
    }
};

PairRemover makeExternRemover(std::shared_ptr<SyntaxTree>& tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics(); // kludge for launching full elaboration
    ExternMapper mapper;
    compilation.getRoot().visit(mapper);
    return PairRemover(std::move(mapper.pairs));
}

bool test(AttemptStats& stats) {
  // Execute ./test.sh tmpFile.
  // On success (zero exit code) replace minimized file with tmp, and return true.
  // On fail (non-zero exit code) return false.
  stats.begin();
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    exit(1);
  } else if (pid == 0) {  // we are inside child
    const char* const argv[] = {files::checkScript.c_str(), files::tmpOutput.c_str(), NULL};
    if (execvp(argv[0], const_cast<char* const*>(argv))) {  // replace child with prog
      std::string err = "bugpoint: failed to lanuch " + files::checkScript;
      perror(err.c_str());
      kill(getppid(), SIGINT); // terminate parent
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
      std::filesystem::copy(files::tmpOutput, files::output,
                            std::filesystem::copy_options::overwrite_existing);
      stats.end(true).report();
      return true;
    }
  }

  return false;  // just to make compiler happy - will never get here
}

bool test(std::shared_ptr<SyntaxTree>& tree, AttemptStats& info) {
  // Write given tree to tmp file and execute ./test.sh tmpFile.
  std::ofstream tmpFile;
  tmpFile.rdbuf()->pubsetbuf(
      0, 0);  // Enable unbuffered io. Has to be called before open to be effective
  tmpFile.open(files::tmpOutput);
  tmpFile << SyntaxPrinter::printFile(*tree);
  return test(info);
}

template <typename T>
bool removeLoop(OneTimeRemover<T> rewriter,
                 std::shared_ptr<SyntaxTree>& tree,
                 std::string stageName,
                 std::string passIdx) {
  bool committed = false;
  bool traversalDone = false;

  while (!traversalDone) {
    auto stats = AttemptStats(passIdx, stageName);;
    auto tmpTree = rewriter.transform(tree, traversalDone, stats);
    if (traversalDone && tmpTree == tree) {
      break;  // no change - no reason to test
    }
    if (test(tmpTree, stats)) {
      tree = tmpTree;
      rewriter.moveStartToSuccesor();
      committed = true;
    } else {
      rewriter.moveStartToChildOrSuccesor();
    }
  }
  return committed;
}

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


bool pass(std::shared_ptr<SyntaxTree>& tree, std::string passIdx = "-") {
  bool commited = false;

  commited |= removeLoop(BodyRemover(), tree, "bodyRemover", passIdx);
  commited |= removeLoop(InstantationRemover(), tree, "instantationRemover", passIdx);
  commited |= removeLoop(BodyPartsRemover(), tree, "bodyPartsRemover", passIdx);
  commited |= removeLoop(makeExternRemover(tree), tree, "externRemover", passIdx);
  commited |= removeLoop(DeclRemover(), tree, "declRemover", passIdx);
  commited |= removeLoop(StatementsRemover(), tree, "statementsRemover", passIdx);
  commited |= removeLoop(ImportsRemover(), tree, "importsRemover", passIdx);
  commited |= removeLoop(ParamAssignRemover(), tree, "paramAssignRemover", passIdx);
  commited |= removeLoop(ContAssignRemover(), tree, "contAssignRemover", passIdx);
  commited |= removeLoop(MemberRemover(), tree, "memberRemover", passIdx);
  commited |= removeLoop(ModportRemover(), tree, "modportRemover", passIdx);

  return commited;
}

void inspect() {
  auto treeOrErr = SyntaxTree::fromFile(files::input);
  if (treeOrErr) {
    auto tree = *treeOrErr;
    AllPrinter printer(2);
    printer.visit(tree->root());
  } else {
    std::cerr << "bugpoint: failed to load " << files::input << " file "<< treeOrErr.error().second << "\n";
    exit(1);
  }
}

void inspectAST() {
  auto treeOrErr = SyntaxTree::fromFile(files::input);
  if (treeOrErr) {
    auto tree = *treeOrErr;
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics(); // kludge for launching full elaboration
    AstPrinter printer;
    printer.visit(compilation.getRoot());
  } else {
    std::cerr << "bugpoint: failed to load " << files::input << " file "<< treeOrErr.error().second << "\n";
    exit(1);
  }
}

bool removeVerilatorConfig() {
  auto info = AttemptStats("-", "verilatorConfigRemover");
  std::ifstream inputFile(files::input);
  std::ofstream testFile(files::tmpOutput);
  std::string line;
  while (std::getline(inputFile, line) && line != "`verilator_config") {
    testFile << line << "\n";
  }
  testFile << std::flush;
  if (line == "`verilator_config") return test(info);
  else return false;
}

void minimize() {
  try {
    std::filesystem::copy(files::input, files::output,
                          std::filesystem::copy_options::overwrite_existing);
  } catch(const std::filesystem::filesystem_error& err) {
    std::cerr << "bugpoint: failed to copy " << files::input << ": " << err.code().message() << "\n";
    exit(1);
  }
  AttemptStats::writeHeader();
  removeVerilatorConfig();

  auto treeOrErr = SyntaxTree::fromFile(files::output);

  if (treeOrErr) {
    auto tree = *treeOrErr;

    int passIdx = 1;
    bool committed;
    do {
      committed = pass(tree, std::to_string(passIdx++));
    } while (committed);

  } else {
      std::cerr << "bugpoint: failed to load " << files::input << " file "<< treeOrErr.error().second << "\n";
      exit(1);
  }
}

int main() {
  // inspect();
  // inspectAST();
  minimize();
}
