// SPDX-License-Identifier: Apache-2.0
#include "IncrementalRewriter.hpp"

class InstantationRemover : public IncrementalRewriter<InstantationRemover> {
   public:
    ShouldVisitChildren handle(const HierarchyInstantiationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool rewriteLoop<InstantationRemover>(std::shared_ptr<SyntaxTree>& tree,
                                               std::string stageName,
                                               std::string passIdx,
                                               SvBugpoint* svBugpoint);
