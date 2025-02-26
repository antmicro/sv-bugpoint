// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRewriter.hpp"

class ParamAssignRemover : public OneTimeRewriter<ParamAssignRemover> {
   public:
    ShouldVisitChildren handle(const ParameterValueAssignmentSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool rewriteLoop<ParamAssignRemover>(std::shared_ptr<SyntaxTree>& tree,
                                              std::string stageName,
                                              std::string passIdx,
                                              SvBugpoint* svBugpoint);
