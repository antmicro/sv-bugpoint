// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRemover.hpp"

class InstantationRemover : public OneTimeRemover<InstantationRemover> {
   public:
    ShouldVisitChildren handle(const HierarchyInstantiationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool removeLoop<InstantationRemover>(std::shared_ptr<SyntaxTree>& tree,
                                              std::string stageName,
                                              std::string passIdx);
