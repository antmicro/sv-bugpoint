#include "OneTimeRemover.hpp"

class ModportRemover : public OneTimeRemover<ModportRemover> {
   public:
    ShouldVisitChildren handle(const ModportDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return DONT_VISIT_CHILDREN;
    }
};

template bool removeLoop<ModportRemover>(std::shared_ptr<SyntaxTree>& tree,
                                         std::string stageName,
                                         std::string passIdx);
