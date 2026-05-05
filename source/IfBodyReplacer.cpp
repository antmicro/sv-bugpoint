// SPDX-License-Identifier: Apache-2.0
#include "IncrementalRewriter.hpp"

namespace {

template <typename TOrig, typename TNew>
void copyIfIndentation(BumpAllocator& alloc, const TOrig& ifNode, TNew& bodyNode) {
    auto ifToken = ifNode.getFirstToken();
    auto* bodyToken = bodyNode.getFirstTokenPtr();
    if (!ifToken || !bodyToken) {
        return;
    }

    auto ifTrivia = ifToken.trivia();
    std::vector<parsing::Trivia> ifIndentation;
    for (size_t idx = ifTrivia.size(); idx > 0; --idx) {
        if (ifTrivia[idx - 1].kind == parsing::TriviaKind::EndOfLine) {
            ifIndentation.assign(ifTrivia.begin() + idx - 1, ifTrivia.end());
            break;
        }
    }
    if (ifIndentation.empty()) {
        return;
    }

    auto mergedTrivia = mergeMovedLeadingTrivia(ifIndentation, bodyToken->trivia());
    auto copiedTrivia = alloc.copyFrom(std::span<const parsing::Trivia>(mergedTrivia));
    *bodyToken = bodyToken->withTrivia(alloc, copiedTrivia);
}

}  // namespace

class IfBodyReplacer : public IncrementalRewriter<IfBodyReplacer> {
   public:
    ShouldVisitChildren handle(const ConditionalStatementSyntax& node, bool isNodeRemovable) {
        replaceIfWithBody(node, *node.statement);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const IfGenerateSyntax& node, bool isNodeRemovable) {
        replaceIfWithBody(node, *node.block);
        return VISIT_CHILDREN;
    }

   private:
    template <typename TIf, typename TBody>
    void replaceIfWithBody(const TIf& node, const TBody& body) {
        if (shouldReplace(node)) {
            auto* bodyClone = deepClone(body, alloc);
            copyIfIndentation(alloc, node, *bodyClone);
            replaceNode(node, *bodyClone);
        }
    }
};

template bool rewriteLoop<IfBodyReplacer>(std::shared_ptr<SyntaxTree>& tree,
                                          std::string stageName,
                                          std::string passIdx,
                                          SvBugpoint* svBugpoint);

class ElseBodyReplacer : public IncrementalRewriter<ElseBodyReplacer> {
   public:
    ShouldVisitChildren handle(const ConditionalStatementSyntax& node, bool isNodeRemovable) {
        replaceIfWithElseBody(node);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const IfGenerateSyntax& node, bool isNodeRemovable) {
        replaceIfWithElseBody(node);
        return VISIT_CHILDREN;
    }

   private:
    template <typename TIf>
    void replaceIfWithElseBody(const TIf& node) {
        if (node.elseClause && shouldReplace(node)) {
            auto* bodyClone = deepClone(*node.elseClause->clause, alloc);
            copyIfIndentation(alloc, node, *bodyClone);
            replaceNode(node, *bodyClone);
        }
    }
};

template bool rewriteLoop<ElseBodyReplacer>(std::shared_ptr<SyntaxTree>& tree,
                                            std::string stageName,
                                            std::string passIdx,
                                            SvBugpoint* svBugpoint);
