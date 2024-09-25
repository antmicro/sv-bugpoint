// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRemover.hpp"

class ContAssignRemover : public OneTimeRemover<ContAssignRemover> {
   public:
    ShouldVisitChildren handle(const ContinuousAssignSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool removeLoop<ContAssignRemover>(std::shared_ptr<SyntaxTree>& tree,
                                            std::string stageName,
                                            std::string passIdx);
