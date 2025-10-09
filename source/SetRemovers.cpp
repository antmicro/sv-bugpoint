// SPDX-License-Identifier: Apache-2.0
#include "SetRemovers.hpp"
#include <slang/ast/ASTVisitor.h>
#include <unordered_map>

class FunctionArgMapper final : public ASTVisitor<FunctionArgMapper, true, true, true> {
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

class PortMapper final : public ASTVisitor<PortMapper, true, true, true> {
    // Builds vector that maps the definitions  and usages of ports
   public:
    class FindConnectionSyntax : public SyntaxVisitor<FindConnectionSyntax> {
        // PortConnection symbol does not have getSyntax() or sourceRange() methods.
        // This visitor finds sourceRange by locating PortConnectionSyntax that contains the same
        // expression as symbol
        enum {
            WAIT_FOR_EXPR,
            REGISTER_PORT_CONN,
            END,
        } state;

        SourceRange exprLoc;

       public:
        SourceRange connLoc;
        FindConnectionSyntax(const PortConnection* symbol)
            : connLoc(SourceRange::NoLocation), state(WAIT_FOR_EXPR) {
            auto expr = symbol ? symbol->getExpression() : nullptr;
            exprLoc = expr ? expr->sourceRange : SourceRange::NoLocation;
            if (exprLoc == SourceRange::NoLocation) {
                state = END;
            }
        }

        void handle(const PortConnectionSyntax& t) {
            visitDefault(t);
            if (state == REGISTER_PORT_CONN) {
                state = END;
                connLoc = t.sourceRange();
            }
        }
        void handle(const ExpressionSyntax& t) {
            if (state == WAIT_FOR_EXPR && t.sourceRange() == exprLoc) {
                state = REGISTER_PORT_CONN;
            }
        }
    };

    SourceRange getPortDefLoc(const Symbol* port) {
        if (port && port->getSyntax() && port->getSyntax()->parent) {
            return port->getSyntax()->parent->sourceRange();
        } else {
            return SourceRange::NoLocation;
        }
    }

    std::vector<SetRemover::RemovalSet> removals;
    void handle(const InstanceSymbol& instance) {
        std::unordered_set<SourceRange> connectedPortDefs;
        for (auto conn : instance.getPortConnections()) {
            if (!conn) {
                continue;
            }

            SourceRange defLocation = getPortDefLoc(&conn->port);
            SourceRange useLocation = SourceRange::NoLocation;

            if (instance.getSyntax()) {
                FindConnectionSyntax finder(conn);
                instance.getSyntax()->visit(finder);
                useLocation = finder.connLoc;
            }

            if (defLocation != SourceRange::NoLocation || useLocation != SourceRange::NoLocation) {
                removals.push_back({defLocation, useLocation});
            }
            if (defLocation != SourceRange::NoLocation) {
                connectedPortDefs.emplace(defLocation);
            }
        }

        for (auto port : instance.body.getPortList()) {
            SourceRange defLocation = getPortDefLoc(port);
            if (defLocation != SourceRange::NoLocation &&
                !connectedPortDefs.contains(defLocation)) {
                removals.push_back({defLocation});
            }
        }
        visitDefault(instance);
    }
};

SetRemover makePortsRemover(std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();  // kludge for launching full elaboration
    PortMapper mapper;
    compilation.getRoot().visit(mapper);
    return SetRemover(std::move(mapper.removals));
}

class ExternMapper final : public ASTVisitor<ExternMapper, true, true, true> {
    // Builds vector that maps the declarations (prototypes) and definitions (implementations) of
    // extern methods
   public:
    std::vector<SetRemover::RemovalSet> removals;

    void handle(const GenericClassDefSymbol& t) {
        ASTVisitor<ExternMapper, true, true, true>::visitDefault(t);
        if (t.numSpecializations() == 0) {
            // in order to visit members of not specialized class we create an artificial
            // specialization
            t.getInvalidSpecialization().visit(*this);
        }
    }

    void handle(const MethodPrototypeSymbol& proto) {
        SourceRange protoLocation = SourceRange::NoLocation;
        SourceRange implLocation = SourceRange::NoLocation;
        ASSERT(proto.getSyntax(), "MethodPrototypeSymbol should have syntax node");
        protoLocation = proto.getSyntax()->sourceRange();

        auto impl = proto.getSubroutine();
        if (impl && impl->getSyntax()) {
            implLocation = impl->getSyntax()->sourceRange();
        }

        if (protoLocation != SourceRange::NoLocation || implLocation != SourceRange::NoLocation) {
            removals.push_back({protoLocation, implLocation});
        }
        visitDefault(proto);
    }
};

SetRemover makeExternRemover(std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();  // kludge for launching full elaboration
    ExternMapper mapper;
    compilation.getRoot().visit(mapper);
    return SetRemover(std::move(mapper.removals));
}

class StructFieldMapper final : public ASTVisitor<StructFieldMapper, true, true, true> {
    // Builds vector that maps the definitions  and initializations of struct fields
   public:
    std::vector<SetRemover::RemovalSet> removals;

    void handle(const StructuredAssignmentPatternExpression& initializer) {
        for (auto setter : initializer.memberSetters) {
            SourceRange defLoc = SourceRange::NoLocation;
            SourceRange initLoc = SourceRange::NoLocation;

            if (setter.member && setter.member->getSyntax() && setter.member->getSyntax()->parent) {
                defLoc = setter.member->getSyntax()->parent->sourceRange();
            }

            if (setter.expr && setter.expr->syntax && setter.expr->syntax->parent) {
                initLoc = setter.expr->syntax->parent->sourceRange();
            }

            removals.push_back({defLoc, initLoc});
        }
        visitDefault(initializer);
    }
};

SetRemover makeStructFieldRemover(std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();  // kludge for launching full elaboration
    StructFieldMapper mapper;
    compilation.getRoot().visit(mapper);
    return SetRemover(std::move(mapper.removals));
}
