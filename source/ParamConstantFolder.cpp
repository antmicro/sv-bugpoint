// SPDX-License-Identifier: Apache-2.0
#include <slang/ast/ASTVisitor.h>
#include "IncrementalRewriter.hpp"

using ParamNameMap = std::unordered_map<std::string, std::set<ConstantValue>>;

class ParamMapper final : public ASTVisitor<ParamMapper, true, true> {
   public:
    ParamNameMap parameters;

    void handle(const ParameterSymbol& parameter) {
        if (parameter.isFromGenvar()) {
            return;
        }
        parameters[trim(parameter.name)].emplace(parameter.getValue());
    }
};

ParamNameMap makeParamInlinesMap(const std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();

    ParamMapper mapper;
    compilation.getRoot().visit(mapper);
    return std::move(mapper.parameters);
}

class ParamConstantFolder : public IncrementalRewriter<ParamConstantFolder> {
   public:
    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree> tree,
                                          AttemptStats& stats,
                                          int n = 1) {
        // transform() copies the syntax tree, so pointers found during earlier attempts are stale.
        // Rebuild the map on every attempt to point at nodes from the current tree.
        parameters = makeParamInlinesMap(tree);
        return IncrementalRewriter<ParamConstantFolder>::transform(tree, stats, n);
    }

    ShouldVisitChildren handle(const ScopedNameSyntax& node, bool isNodeRemovable) {
        if (node.getChildCount() == 0) {
            return DONT_VISIT_CHILDREN;
        }
        // extract the last child of 'namespace::param' syntax (in this case 'param')
        // and queue the whole expression for removal
        const ConstTokenOrSyntax& child = node.getChild(node.getChildCount() - 1);
        if (child.isNode() && child.node()->isKind(SyntaxKind::IdentifierName)) {
            const IdentifierNameSyntax* const identifier =
                child.node()->as_if<IdentifierNameSyntax>();
            if (identifier) {
                return tryReplace(node, *identifier);
            }
        }
        return VISIT_CHILDREN;
    }
    ShouldVisitChildren handle(const IdentifierNameSyntax& node, bool isNodeRemovable) {
        return tryReplace(node, node);
    }

   private:
    // map parameter names with their declared constant value
    ParamNameMap parameters;

    ShouldVisitChildren tryReplace(const SyntaxNode& node, const IdentifierNameSyntax& paramId) {
        // replace 'node' with a constant bound to a parameter of same identifier as 'paramId'
        auto it = parameters.find(trim(paramId.toString()));
        if (it != parameters.end() && !it->second.empty()) {
            // Extract only the first element. If it has wrong value, we hope that its declaration
            // will be removed after this pass.
            const ConstantValue& constant = *it->second.begin();
            replaceNode(node, parse(constant.toString()));
            return DONT_VISIT_CHILDREN;
        }
        return VISIT_CHILDREN;
    }
};

template bool rewriteLoop<ParamConstantFolder>(std::shared_ptr<SyntaxTree>& tree,
                                               std::string stageName,
                                               std::string passIdx,
                                               SvBugpoint* svBugpoint);