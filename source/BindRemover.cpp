// SPDX-License-Identifier: Apache-2.0
#include "IncrementalRewriter.hpp"

class BindRemover : public IncrementalRewriter<BindRemover> {
   public:
    ShouldVisitChildren handle(const BindDirectiveSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool rewriteLoop<BindRemover>(std::shared_ptr<SyntaxTree>& tree,
                                       std::string stageName,
                                       std::string passIdx,
                                       SvBugpoint* svBugpoint);
