#include "OneTimeRemover.hpp"

class StatementsRemover : public OneTimeRemover<StatementsRemover> {
   public:
    ShouldVisitChildren handle(const ProceduralBlockSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
    ShouldVisitChildren handle(const StatementSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
    ShouldVisitChildren handle(const LocalVariableDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
};

template bool removeLoop<StatementsRemover>(std::shared_ptr<SyntaxTree>& tree,
                                            std::string stageName,
                                            std::string passIdx);
