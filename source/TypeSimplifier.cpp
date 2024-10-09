#include "OneTimeRewriter.hpp"

class IsPrimitive : public SyntaxVisitor<IsPrimitive> {
   public:
    // We don't visit the children, so it might not be obvious why we're using a Visitor.
    // We do it so we can use SyntaxNode::visit for dynamic type dispatch.
    bool isPrimitive;

    template <typename T>
    void handle(const T& t) {
        isPrimitive = false;
    }

    void handle(const IntegerTypeSyntax& t) { isPrimitive = true; }

    void handle(const KeywordTypeSyntax& t) { isPrimitive = true; }

    void handle(const ImplicitTypeSyntax& t) { isPrimitive = true; }
};

class TypeSimplifier : public OneTimeRewriter<TypeSimplifier> {
    // replace references to user-defined types with int
   public:
    bool canSimplify(not_null<const DataTypeSyntax*> node) {
        IsPrimitive checker;
        node->visit(checker);
        return !checker.isPrimitive;
    }

    IntegerTypeSyntax* makeIntNode(SourceLocation location) {
        auto token =
            Token(alloc, parsing::TokenKind::IntKeyword, {&SingleSpace, 1}, "int", location);
        auto node = IntegerTypeSyntax(SyntaxKind::IntType, token, {},
                                      SyntaxList<VariableDimensionSyntax>({}));
        return alloc.emplace<IntegerTypeSyntax>(node);
    }

    ShouldVisitChildren handle(const DataTypeSyntax& node, bool isNodeRemovable) {
        if (state == REMOVAL_ALLOWED && canSimplify(&node)) {
            replaceNode(node, *makeIntNode(node.sourceRange().start()));
        }
        return VISIT_CHILDREN;
    }
};

template bool rewriteLoop<TypeSimplifier>(std::shared_ptr<SyntaxTree>& tree,
                                          std::string stageName,
                                          std::string passIdx);
