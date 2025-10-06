// SPDX-License-Identifier: Apache-2.0
#include "SetRemovers.hpp"
#include <slang/ast/ASTVisitor.h>
#include <unordered_map>

class FunctionArgMapper : public ASTVisitor<FunctionArgMapper, true, true, true> {
    // Builds vector that maps the argument in definition to all calls
   public:
    std::vector<SetRemover::RemovalSet> removals;

    void handle(const SubroutineSymbol& subroutine) {
        if (shouldProcess(subroutine)) {
            for (const FormalArgumentSymbol* arg : subroutine.getArguments()) {
                registerFormal(arg);
            }
        }

        visitDefault(subroutine);
    }

    void handle(const MethodPrototypeSymbol& prototype) {
        if (shouldProcess(prototype)) {
            for (const FormalArgumentSymbol* arg : prototype.getArguments()) {
                registerFormal(arg);
            }
        }

        visitDefault(prototype);
    }

    void handle(const CallExpression& call) {
        const SubroutineSymbol* subroutine = getSubroutine(call);
        if (!subroutine || !shouldProcess(*subroutine)) {
            visitDefault(call);
            return;
        }

        auto formals = subroutine->getArguments();
        auto actuals = call.arguments();
        size_t count = std::min(formals.size(), actuals.size());
        for (size_t idx = 0; idx < count; idx++) {
            size_t removalIdx = registerFormal(formals[idx]);
            if (removalIdx == invalidIndex) {
                continue;
            }
            const SyntaxNode* argNode = getArgumentNode(actuals[idx]);
            if (argNode && argNode->sourceRange() != SourceRange::NoLocation) {
                removals[removalIdx].push_back(argNode->sourceRange());
            }
        }

        visitDefault(call);
    }

   private:
    static constexpr size_t invalidIndex = std::numeric_limits<size_t>::max();
    std::unordered_map<const FormalArgumentSymbol*, size_t> indexByFormal;

    size_t registerFormal(const FormalArgumentSymbol* formal) {
        if (!formal) {
            return invalidIndex;
        }

        if (auto it = indexByFormal.find(formal); it != indexByFormal.end()) {
            return it->second;
        }

        const SyntaxNode* node = getFormalNode(*formal);
        if (!node || node->sourceRange() == SourceRange::NoLocation) {
            indexByFormal.emplace(formal, invalidIndex);
            return invalidIndex;
        }

        SetRemover::RemovalSet set;
        set.push_back(node->sourceRange());
        removals.push_back(std::move(set));
        size_t idx = removals.size() - 1;
        indexByFormal.emplace(formal, idx);
        return idx;
    }

    static const SyntaxNode* getFormalNode(const FormalArgumentSymbol& formal) {
        if (auto syntax = formal.getSyntax()) {
            if (syntax->parent) {
                return syntax->parent;
            }
            return syntax;
        }
        return nullptr;
    }

    static const SyntaxNode* getArgumentNode(const Expression* expr) {
        if (!expr || !expr->syntax) {
            return nullptr;
        }

        const SyntaxNode* node = expr->syntax;
        while (node) {
            if (node->kind == SyntaxKind::OrderedArgument ||
                node->kind == SyntaxKind::NamedArgument) {
                return node;
            }
            node = node->parent;
        }
        return nullptr;
    }

    static const SubroutineSymbol* getSubroutine(const CallExpression& call) {
        if (call.isSystemCall()) {
            return nullptr;
        }
        if (std::holds_alternative<const SubroutineSymbol*>(call.subroutine)) {
            return std::get<const SubroutineSymbol*>(call.subroutine);
        }
        return nullptr;
    }

    static bool shouldProcess(const SubroutineSymbol& subroutine) {
        return subroutine.subroutineKind == SubroutineKind::Function ||
               subroutine.subroutineKind == SubroutineKind::Task;
    }

    static bool shouldProcess(const MethodPrototypeSymbol& prototype) {
        return prototype.subroutineKind == SubroutineKind::Function ||
               prototype.subroutineKind == SubroutineKind::Task;
    }
};

SetRemover makeFunctionArgRemover(std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();
    FunctionArgMapper mapper;
    compilation.getRoot().visit(mapper);
    return SetRemover(std::move(mapper.removals));
}
