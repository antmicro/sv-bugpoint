// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <slang/syntax/SyntaxVisitor.h>
#include "Utils.hpp"

class SetRemover : public SyntaxRewriter<SetRemover> {
    // each transform yields removal of a set of nodes (based on locations in supplied sets list)
   public:
    using RemovalSet = std::vector<SourceRange>;

    std::vector<RemovalSet> removals;
    std::unordered_set<SourceRange> pendingNodes;
    std::string removedTypeInfo;

    SetRemover(std::vector<RemovalSet>&& removals) : removals(removals) {}

    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree>& tree,
                                          bool& traversalDone,
                                          AttemptStats& stats) {
        if (removals.empty()) {
            traversalDone = true;
            return tree;
        }
        RemovalSet removal = removals.back();
        removals.pop_back();
        pendingNodes.clear();
        removedTypeInfo = "";
        for (const auto node : removal) {
            if (node != SourceRange::NoLocation) {
                pendingNodes.insert(node);
            }
        }
        if (pendingNodes.empty()) {
            return transform(tree, traversalDone, stats);
        }

        auto tree2 = SyntaxRewriter<SetRemover>::transform(tree);

        if (!pendingNodes.empty()) {
            return transform(tree, traversalDone, stats);
        }

        stats.typeInfo = removedTypeInfo;
        traversalDone = removals.empty();
        return tree2;
    }

    /// The default handler invoked when no visit() method is overridden for a particular type.
    /// Will visit all child nodes by default.
    template <typename T>
    void visitDefault(T&& node) {
        for (uint32_t i = 0; i < node.getChildCount(); i++) {
            auto child = node.childNode(i);
            if (child) {
                child->visit(*this, node.isChildOptional(i));
            }
        }
    }

    template <typename T>
    void logType() {
        std::cerr << STRINGIZE_NODE_TYPE(T) << "\n";
        removedTypeInfo += (removedTypeInfo.empty() ? "" : ",") + STRINGIZE_NODE_TYPE(T);
    }

    template <typename T>
    void visit(T&& node, bool isNodeRemovable = true) {
        auto it = pendingNodes.find(node.sourceRange());
        if (it != pendingNodes.end() && isNodeRemovable) {
            logType<T>();
            std::cerr << prefixLines(node.toString(), "-") << "\n";
            remove(node);
            pendingNodes.erase(it);
            return;
        }
        visitDefault(node);
    }
};

SetRemover makeFunctionArgRemover(std::shared_ptr<SyntaxTree> tree);
