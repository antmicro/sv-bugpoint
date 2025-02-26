// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRewriter.hpp"

class ContAssignRemover : public OneTimeRewriter<ContAssignRemover> {
   public:
    ShouldVisitChildren handle(const ContinuousAssignSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool rewriteLoop<ContAssignRemover>(std::shared_ptr<SyntaxTree>& tree,
                                             std::string stageName,
                                             std::string passIdx,
                                             SvBugpoint* svBugpoint);
