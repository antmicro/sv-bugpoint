// SPDX-License-Identifier: Apache-2.0
#include <slang/ast/ASTVisitor.h>
#include <unordered_map>
#include "IncrementalRewriter.hpp"

static bool hasExternQualifier(const TokenList& qualifiers) {
    for (const auto token : qualifiers) {
        if (token.kind == parsing::TokenKind::ExternKeyword) {
            return true;
        }
    }
    return false;
}

static std::vector<parsing::Trivia> mergeExternLeadingTrivia(
    const std::vector<parsing::Trivia>& movedTrivia,
    std::span<const parsing::Trivia> originalTrivia) {
    std::vector<parsing::Trivia> mergedTrivia;
    mergedTrivia.reserve(movedTrivia.size() + originalTrivia.size());
    mergedTrivia.insert(mergedTrivia.end(), movedTrivia.begin(), movedTrivia.end());

    size_t firstTrivia = 0;
    if (!movedTrivia.empty() && !originalTrivia.empty() &&
        originalTrivia.front().kind == parsing::TriviaKind::Whitespace) {
        // Remove the single separator space that used to be between `extern` and
        // the next token (for example, `extern virtual` -> `virtual`).
        firstTrivia = 1;
    }

    mergedTrivia.insert(mergedTrivia.end(), originalTrivia.begin() + firstTrivia,
                        originalTrivia.end());
    return mergedTrivia;
}

struct ExternInlineCandidate {
    // Clone the declaration body, but remove the whole source node that owns it.
    not_null<const FunctionDeclarationSyntax*> implementationDecl;
    not_null<const SyntaxNode*> implementationRemovalNode;
    std::string methodName;
};

using ExternInlineMap =
    std::unordered_map<const ClassMethodPrototypeSyntax*, ExternInlineCandidate>;

class ExternInlineMapper final : public ASTVisitor<ExternInlineMapper, true, true, true> {
   public:
    ExternInlineMap candidates;

    void handle(const GenericClassDefSymbol& node) {
        ASTVisitor<ExternInlineMapper, true, true, true>::visitDefault(node);
        if (node.numSpecializations() == 0) {
            // In order to visit members of not specialized class we create an artificial
            // specialization.
            node.getInvalidSpecialization().visit(*this);
        }
    }

    void handle(const MethodPrototypeSymbol& proto) {
        const auto* protoSyntax =
            proto.getSyntax() ? proto.getSyntax()->as_if<ClassMethodPrototypeSyntax>() : nullptr;
        const auto* impl = proto.getSubroutine();
        const auto* implSyntax = impl ? impl->getSyntax() : nullptr;
        const auto* implDecl =
            implSyntax ? implSyntax->as_if<FunctionDeclarationSyntax>() : nullptr;
        const SyntaxNode* implNode = implSyntax;

        if (const auto* classMethodDecl =
                implSyntax ? implSyntax->as_if<ClassMethodDeclarationSyntax>() : nullptr) {
            implDecl = classMethodDecl->declaration;
            implNode = classMethodDecl;
        }

        if (protoSyntax && hasExternQualifier(protoSyntax->qualifiers) && implDecl && implNode &&
            implNode != protoSyntax &&
            (implDecl->kind == SyntaxKind::FunctionDeclaration ||
             implDecl->kind == SyntaxKind::TaskDeclaration)) {
            candidates.emplace(protoSyntax,
                               ExternInlineCandidate{implDecl, implNode, std::string(proto.name)});
        }

        visitDefault(proto);
    }
};

static ExternInlineMap makeExternInlineMap(const std::shared_ptr<SyntaxTree>& tree) {
    Compilation compilation;
    compilation.addSyntaxTree(tree);
    compilation.getAllDiagnostics();

    ExternInlineMapper mapper;
    compilation.getRoot().visit(mapper);
    return std::move(mapper.candidates);
}

class ExternInliner : public IncrementalRewriter<ExternInliner> {
   public:
    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree> tree,
                                          AttemptStats& stats,
                                          int n = 1) {
        // transform() copies the syntax tree, so pointers found during earlier attempts are stale.
        // Rebuild the map on every attempt to point at nodes from the current tree.
        candidateByPrototype = makeExternInlineMap(tree);
        currentClassNode = nullptr;
        return IncrementalRewriter<ExternInliner>::transform(tree, stats, n);
    }

    ShouldVisitChildren handle(const ClassDeclarationSyntax& node, bool isNodeRemovable) {
        currentClassNode = &node;
        visitDefault(node);
        currentClassNode = nullptr;
        return DONT_VISIT_CHILDREN;
    }

    ShouldVisitChildren handle(const ClassMethodPrototypeSyntax& node, bool isNodeRemovable) {
        if (!isNodeRemovable) {
            return VISIT_CHILDREN;
        }

        if (!currentClassNode) {
            return VISIT_CHILDREN;
        }

        auto it = candidateByPrototype.find(&node);
        if (it == candidateByPrototype.end()) {
            return VISIT_CHILDREN;
        }

        bool removedExtern = false;
        auto& replacement =
            createInlinedMember(node, *it->second.implementationDecl, removedExtern);
        if (!removedExtern) {
            return VISIT_CHILDREN;
        }

        if (!shouldReplace(node)) {
            return VISIT_CHILDREN;
        }

        logType<ClassMethodPrototypeSyntax>();
        std::cerr << prefixLines(node.toString(), "-") << "\n";
        std::cerr << prefixLines(it->second.implementationRemovalNode->toString(), "-") << "\n";
        std::cerr << prefixLines(replacement.toString(), "+") << "\n";

        if (!it->second.methodName.empty()) {
            rewrittenTypeInfo = it->second.methodName;
        }

        // Remove the extern declaration and place the inlined method at the end of the class,
        // so member accesses remain valid even if fields are declared below the prototype.
        insertAtBack(currentClassNode->items, replacement);
        remove(node);
        remove(*it->second.implementationRemovalNode);

        checkPoints.push_back({node.sourceRange()});
        state = REGISTER_CHILD;
        return DONT_VISIT_CHILDREN;
    }

   private:
    ExternInlineMap candidateByPrototype;
    const ClassDeclarationSyntax* currentClassNode = nullptr;

    TokenList& cloneQualifiersWithoutExtern(const TokenList& qualifiers,
                                            bool& removedExtern,
                                            std::vector<parsing::Trivia>& leadingExternTrivia) {
        std::vector<Token> out;
        out.reserve(qualifiers.size());
        for (const auto token : qualifiers) {
            if (token.kind == parsing::TokenKind::ExternKeyword) {
                removedExtern = true;
                if (out.empty()) {
                    for (const auto trivia : token.trivia()) {
                        leadingExternTrivia.push_back(trivia);
                    }
                }
                continue;
            }

            if (out.empty() && !leadingExternTrivia.empty()) {
                auto mergedTrivia = mergeExternLeadingTrivia(leadingExternTrivia, token.trivia());
                auto copiedTrivia = alloc.copyFrom(std::span<const parsing::Trivia>(mergedTrivia));
                out.push_back(token.withTrivia(alloc, copiedTrivia));
                leadingExternTrivia.clear();
                continue;
            }

            out.push_back(token.deepClone(alloc));
        }

        auto copied = alloc.copyFrom(std::span<const Token>(out));
        return *alloc.emplace<TokenList>(copied);
    }

    ClassMethodDeclarationSyntax& createInlinedMember(
        const ClassMethodPrototypeSyntax& prototype,
        const FunctionDeclarationSyntax& implementation,
        bool& removedExtern) {
        std::vector<parsing::Trivia> leadingExternTrivia;
        auto& qualifiers =
            cloneQualifiersWithoutExtern(prototype.qualifiers, removedExtern, leadingExternTrivia);

        auto& newFunctionDecl = factory.functionDeclaration(
            implementation.kind, *deepClone(implementation.attributes, alloc),
            *deepClone(*prototype.prototype, alloc), prototype.semi.deepClone(alloc),
            *deepClone(implementation.items, alloc), implementation.end.deepClone(alloc),
            implementation.endBlockName ? deepClone(*implementation.endBlockName, alloc) : nullptr);

        auto& classMethod = factory.classMethodDeclaration(*deepClone(prototype.attributes, alloc),
                                                           qualifiers, newFunctionDecl);

        if (!leadingExternTrivia.empty()) {
            auto* firstTok = classMethod.getFirstTokenPtr();
            ASSERT(firstTok, "inlined class method must have a first token");

            auto mergedTrivia = mergeExternLeadingTrivia(leadingExternTrivia, firstTok->trivia());
            auto copiedTrivia = alloc.copyFrom(std::span<const parsing::Trivia>(mergedTrivia));
            *firstTok = firstTok->withTrivia(alloc, copiedTrivia);
        }

        return classMethod;
    }
};

template bool rewriteLoop<ExternInliner>(std::shared_ptr<SyntaxTree>& tree,
                                         std::string stageName,
                                         std::string passIdx,
                                         SvBugpoint* svBugpoint);
