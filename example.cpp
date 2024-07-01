#include "slang/syntax/SyntaxNode.h"
#include "slang/util/BumpAllocator.h"
#include <slang/text/SourceManager.h>
#include <slang/text/SourceLocation.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <iostream>

#define DERIVED static_cast<TDerived*>(this)

template<typename TDerived>
class OneTimeRewriter: public slang::syntax::SyntaxRewriter<TDerived> {
  public:
    enum State {
      SKIP_TO_START,
      HANDLE_ALLOWED,
      REGISTER_SUCCESSOR,
      SKIP_TO_END,
    };

    // we store SyntaxNode* despite the fact that we are only intrested in SyntaxLocation
    // (SyntaxLocation does not have assign operator implemented)
    const slang::syntax::SyntaxNode* startPoint = nullptr;
    const slang::syntax::SyntaxNode* removed = nullptr;
    const slang::syntax::SyntaxNode* removedSuccessor = nullptr;

    State state = HANDLE_ALLOWED;

    template<typename T>
    void visit(T&& t) {
        if(state == SKIP_TO_START && t.sourceRange() == startPoint->sourceRange()) {
            state = HANDLE_ALLOWED;
        }

        if(state == REGISTER_SUCCESSOR) {
          removedSuccessor = &t;
          state = SKIP_TO_END;
          return;
        }

        if(state == SKIP_TO_END) {
          return;
        }


        if constexpr (requires { DERIVED->handle(t); }) {
            if(state == HANDLE_ALLOWED) DERIVED->handle(t);
        }
        else {
            DERIVED->visitDefault(t);
        }
  }

  std::shared_ptr<slang::syntax::SyntaxTree> transform(const std::shared_ptr<slang::syntax::SyntaxTree>& tree) {
      auto tree2 = slang::syntax::SyntaxRewriter<TDerived>::transform(tree);
      // I'm not sure about what intended behavior is, but head of SyntaxRewriter's allocator is nulled after traversal,
      // leading to NULL dereference when rewriter is reused. This is dirty work around this. TODO: examine it more carefully.
      this->alloc = slang::BumpAllocator();
      return tree2;
  }
};

class MyRemover: public OneTimeRewriter<MyRemover> {
  public:
  void handle(const slang::syntax::LoopGenerateSyntax& node) {
      std::cerr << node.toString() << "\n";
      remove(node);
      removed = &node;
      state = REGISTER_SUCCESSOR;
  }
};


bool test(int i) {
  // Just for PoC that we can remove nodes granularly
  return i != 5;
}

void removeLoop(std::shared_ptr<slang::syntax::SyntaxTree> tree) {
  MyRemover rewriter;
  bool notEnd = true;
  int i = 0;
  while(notEnd) {
    rewriter.startPoint = rewriter.removedSuccessor;
    auto tmpTree = rewriter.transform(tree);
    if(test(i)) {
      tree = tmpTree;
    }
    notEnd = rewriter.state == MyRemover::SKIP_TO_END;
    rewriter.state = MyRemover::SKIP_TO_START;
    i++;
  }
  std::cout << slang::syntax::SyntaxPrinter::printFile(*tree);
}

int main() {
  MyRemover rewriter;
  auto treeOrErr = slang::syntax::SyntaxTree::fromFile("uvm.sv");
  if (treeOrErr) {
      auto tree = *treeOrErr;
      removeLoop(tree);
  }
  else {
      /* do something with result.error() */
  }
}
