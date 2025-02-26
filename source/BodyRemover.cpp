// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRewriter.hpp"

class BodyRemover : public OneTimeRewriter<BodyRemover> {
   public:
    ShouldVisitChildren handle(const ClassDeclarationSyntax& node, bool isNodeRemovable) {
        removeChildList(node, node.items);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const FunctionDeclarationSyntax& node, bool isNodeRemovable) {
        removeChildList(node, node.items);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ModuleDeclarationSyntax& node, bool isNodeRemovable) {
        removeChildList(node, node.members);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const BlockStatementSyntax& node, bool isNodeRemovable) {
        removeChildList(node, node.items);
        return VISIT_CHILDREN;
    }
};

template bool rewriteLoop<BodyRemover>(std::shared_ptr<SyntaxTree>& tree,
                                       std::string stageName,
                                       std::string passIdx,
                                       SvBugpoint* svBugpoint);
