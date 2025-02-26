// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRewriter.hpp"

class ModuleRemover : public OneTimeRewriter<ModuleRemover> {
   public:
    ShouldVisitChildren handle(const ModuleDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool rewriteLoop<ModuleRemover>(std::shared_ptr<SyntaxTree>& tree,
                                         std::string stageName,
                                         std::string passIdx,
                                         SvBugpoint* svBugpoint);
