// SPDX-License-Identifier: Apache-2.0
#include "IncrementalRewriter.hpp"

class ImportsRemover : public IncrementalRewriter<ImportsRemover> {
   public:
    ShouldVisitChildren handle(const PackageImportDeclarationSyntax& node, bool isNodeRemovable) {
        removeNode(node, isNodeRemovable);
        return VISIT_CHILDREN;
    }
};

template bool rewriteLoop<ImportsRemover>(std::shared_ptr<SyntaxTree>& tree,
                                          std::string stageName,
                                          std::string passIdx,
                                          SvBugpoint* svBugpoint);
