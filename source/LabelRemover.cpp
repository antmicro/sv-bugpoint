// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRewriter.hpp"

class LabelRemover : public OneTimeRewriter<LabelRemover> {
   public:
    ShouldVisitChildren handle(const NamedBlockClauseSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool rewriteLoop<LabelRemover>(std::shared_ptr<SyntaxTree>& tree,
                                        std::string stageName,
                                        std::string passIdx,
                                        SvBugpoint* svBugpoint);
