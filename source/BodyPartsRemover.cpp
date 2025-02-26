// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRewriter.hpp"

class BodyPartsRemover : public OneTimeRewriter<BodyPartsRemover> {
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

template bool rewriteLoop<BodyPartsRemover>(std::shared_ptr<SyntaxTree>& tree,
                                            std::string stageName,
                                            std::string passIdx,
                                            SvBugpoint* svBugpoint);
