#include "OneTimeRemover.hpp"

class BodyPartsRemover : public OneTimeRemover<BodyPartsRemover> {
   public:
    ShouldVisitChildren handle(const LoopGenerateSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
    ShouldVisitChildren handle(const ConcurrentAssertionMemberSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
    ShouldVisitChildren handle(const ElseClauseSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
};

template bool removeLoop<BodyPartsRemover>(std::shared_ptr<SyntaxTree>& tree,
                                           std::string stageName,
                                           std::string passIdx);
