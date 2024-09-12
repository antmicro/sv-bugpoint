#include "PairRemovers.hpp"
#include <slang/ast/ASTVisitor.h>
#include <unordered_set>
#include "utils.hpp"

class PortMapper : public ASTVisitor<PortMapper, true, true, true> {
    // Builds vector that maps the definitions  and usages of ports
   public:
    class FindConnectionSyntax : public SyntaxVisitor<FindConnectionSyntax> {
       public:
        // PortConnection symbol does not have getSyntax() or sourceRange() methods.
        // This visitor finds sourceRange by locating PortConnectionSyntax that contains the same
        // expression as symbol
        enum {
            WAIT_FOR_EXPR,
            REGISTER_PORT_CONN,
            END,
        } state;

        SourceRange exprLoc;
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

    std::vector<std::pair<SourceRange, SourceRange>> pairs;
    void handle(const InstanceSymbol& instance) {
        std::unordered_set<SourceRange> connectedPortDefs;
        for (auto conn : instance.getPortConnections()) {
            if (!conn)
                continue;

            SourceRange defLocation = getPortDefLoc(&conn->port);
            SourceRange useLocation = SourceRange::NoLocation;

            if (instance.getSyntax()) {
                FindConnectionSyntax finder(conn);
                instance.getSyntax()->visit(finder);
                useLocation = finder.connLoc;
            }

            if (defLocation != SourceRange::NoLocation || useLocation != SourceRange::NoLocation) {
                pairs.push_back({defLocation, useLocation});
            }
            if (defLocation != SourceRange::NoLocation) {
                connectedPortDefs.emplace(defLocation);
            }
        }

        for (auto port : instance.body.getPortList()) {
            SourceRange defLocation = getPortDefLoc(port);
            if (defLocation != SourceRange::NoLocation &&
                !connectedPortDefs.contains(defLocation)) {
                pairs.push_back({defLocation, SourceRange::NoLocation});
            }
        }
        visitDefault(instance);
    }
};

PairRemover makePortsRemover(std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();  // kludge for launching full elaboration
    PortMapper mapper;
    compilation.getRoot().visit(mapper);
    return PairRemover(std::move(mapper.pairs));
}

class ExternMapper : public ASTVisitor<ExternMapper, true, true, true> {
    // Builds vector that maps the declarations (prototypes) and definitions (implementations) of
    // extern methods
   public:
    std::vector<std::pair<SourceRange, SourceRange>> pairs;

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
        if (proto.getSyntax()) {
            protoLocation = proto.getSyntax()->sourceRange();
        } else {
            exit(1);
        }

        auto impl = proto.getSubroutine();
        if (impl && impl->getSyntax()) {
            implLocation = impl->getSyntax()->sourceRange();
        }

        if (protoLocation != SourceRange::NoLocation || implLocation != SourceRange::NoLocation) {
            pairs.push_back({protoLocation, implLocation});
        }
        visitDefault(proto);
    }
};

PairRemover makeExternRemover(std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();  // kludge for launching full elaboration
    ExternMapper mapper;
    compilation.getRoot().visit(mapper);
    return PairRemover(std::move(mapper.pairs));
}

class StructFieldMapper : public ASTVisitor<StructFieldMapper, true, true, true> {
    // Builds vector that maps the definitions  and initializations of struct fields
   public:
    std::vector<std::pair<SourceRange, SourceRange>> pairs;

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

            pairs.push_back({defLoc, initLoc});
        }
        visitDefault(initializer);
    }
};

PairRemover makeStructFieldRemover(std::shared_ptr<SyntaxTree> tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();  // kludge for launching full elaboration
    StructFieldMapper mapper;
    compilation.getRoot().visit(mapper);
    return PairRemover(std::move(mapper.pairs));
}
