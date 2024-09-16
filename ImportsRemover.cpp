#include "OneTimeRemover.hpp"

class ImportsRemover : public OneTimeRemover<ImportsRemover> {
   public:
    ShouldVisitChildren handle(const PackageImportDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
};

template bool removeLoop<ImportsRemover>(std::shared_ptr<SyntaxTree>& tree,
                                         std::string stageName,
                                         std::string passIdx);
