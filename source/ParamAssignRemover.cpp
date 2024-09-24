// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRemover.hpp"

class ParamAssignRemover : public OneTimeRemover<ParamAssignRemover> {
   public:
    ShouldVisitChildren handle(const ParameterValueAssignmentSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool removeLoop<ParamAssignRemover>(std::shared_ptr<SyntaxTree>& tree,
                                             std::string stageName,
                                             std::string passIdx);
