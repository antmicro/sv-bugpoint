#include "slang/syntax/SyntaxNode.h"
#include <slang/text/SourceManager.h>
#include <slang/text/SourceLocation.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxPrinter.h>
#include <iostream>

class CustomRewriter: public slang::syntax::SyntaxRewriter<CustomRewriter> {
  public:
  void handle(const slang::syntax::LoopGenerateSyntax& node) {
      remove(node);
      visitDefault(node);
  }
};

int main() {
  CustomRewriter rewriter;
  auto treeOrErr = slang::syntax::SyntaxTree::fromFile("uvm.sv");
  if (treeOrErr) {
      auto tree = *treeOrErr;
      auto modifiedTree = rewriter.transform(tree);
      std::cout << slang::syntax::SyntaxPrinter::printFile(*modifiedTree);
  }
  else {
      /* do something with result.error() */
  }
}
