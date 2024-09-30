// SPDX-License-Identifier: Apache-2.0
#include <slang/syntax/SyntaxVisitor.h>
#include "Utils.hpp"

class PairRemover : public SyntaxRewriter<PairRemover> {
    // each tranform yields removal of pair of nodes (based on locations in suplied pairs list)
   public:
    std::vector<std::pair<SourceRange, SourceRange>> pairs;
    std::pair<SourceRange, SourceRange> searchedPair;
    std::string removedTypeInfo;

    PairRemover(std::vector<std::pair<SourceRange, SourceRange>>&& pairs) : pairs(pairs) {}

    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree> tree,
                                          bool& traversalDone,
                                          AttemptStats& stats) {
        if (pairs.empty()) {
            traversalDone = true;
            return tree;
        }
        searchedPair = pairs.back();
        pairs.pop_back();
        removedTypeInfo = "";
        auto tree2 = SyntaxRewriter<PairRemover>::transform(tree);
        stats.typeInfo = removedTypeInfo;
        // maybe do it in while(not changed) loop
        traversalDone = pairs.empty();
        return tree2;
    }

    /// The default handler invoked when no visit() method is overridden for a particular type.
    /// Will visit all child nodes by default.
    template <typename T>
    void visitDefault(T&& node) {
        for (uint32_t i = 0; i < node.getChildCount(); i++) {
            auto child = node.childNode(i);
            if (child)
                child->visit(*this, node.isChildOptional(i));
        }
    }

    template <typename T>
    void logType() {
        std::cerr << STRINGIZE_NODE_TYPE(T) << "\n";
        removedTypeInfo += (removedTypeInfo.empty() ? "" : ",") + STRINGIZE_NODE_TYPE(T);
    }

    template <typename T>
    void visit(T&& node, bool isNodeRemovable = true) {
        bool found =
            node.sourceRange() == searchedPair.first || node.sourceRange() == searchedPair.second;
        if (isNodeRemovable && found && node.sourceRange() != SourceRange::NoLocation) {
            logType<T>();
            std::cerr << prefixLines(node.toString(), "-") << "\n";
            remove(node);
        }

        visitDefault(node);
    }
};

PairRemover makePortsRemover(std::shared_ptr<SyntaxTree> tree);
PairRemover makeExternRemover(std::shared_ptr<SyntaxTree> tree);
PairRemover makeStructFieldRemover(std::shared_ptr<SyntaxTree> tree);
