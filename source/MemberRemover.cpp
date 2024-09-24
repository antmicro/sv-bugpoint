// SPDX-License-Identifier: Apache-2.0
#include "OneTimeRemover.hpp"

class MemberRemover : public OneTimeRemover<MemberRemover> {
   public:
    ShouldVisitChildren handle(const DataDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const NetDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const StructUnionMemberSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const DeclaratorSyntax& node,
                               bool isNodeRemovable) {  // a.o. enum fields
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ParameterDeclarationStatementSyntax& node,
                               bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ClassPropertyDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ParameterDeclarationBaseSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool removeLoop<MemberRemover>(std::shared_ptr<SyntaxTree>& tree,
                                        std::string stageName,
                                        std::string passIdx);
