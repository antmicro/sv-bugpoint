// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRewriter.hpp"

class ModportRemover : public OneTimeRewriter<ModportRemover> {
   public:
    ShouldVisitChildren handle(const ModportDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool rewriteLoop<ModportRemover>(std::shared_ptr<SyntaxTree>& tree,
                                         std::string stageName,
                                         std::string passIdx);
