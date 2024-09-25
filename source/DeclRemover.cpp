// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRemover.hpp"

class DeclRemover : public OneTimeRemover<DeclRemover> {
   public:
    ShouldVisitChildren handle(const FunctionDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ModuleDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const TypedefDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ForwardTypedefDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ClassDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ImplementsClauseSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ExtendsClauseSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ConstraintDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ClassMethodPrototypeSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ClassMethodDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
};

template bool removeLoop<DeclRemover>(std::shared_ptr<SyntaxTree>& tree,
                                      std::string stageName,
                                      std::string passIdx);
