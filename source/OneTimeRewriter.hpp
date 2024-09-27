// SPDX-License-Identifier: Apache-2.0
#include <slang/syntax/SyntaxVisitor.h>
#include <iosfwd>
#include "Utils.hpp"

#define DERIVED static_cast<TDerived*>(this)

template <typename TDerived>
class OneTimeRewriter : public SyntaxRewriter<TDerived> {
    // Incremental node rewriter - each transform() yields one rewrite at most
   public:
    enum State {
        SKIP_TO_START,
        REMOVAL_ALLOWED,
        REGISTER_CHILD,
        EXIT_REWRITE_POINT,
        REGISTER_SUCCESSOR,
        SKIP_TO_END,
    };

    enum ShouldVisitChildren {
        VISIT_CHILDREN,
        DONT_VISIT_CHILDREN,
    };

    SourceRange startPoint;

    SourceRange rewritePoint;
    SourceRange rewritePointChildren;
    SourceRange rewritePointSuccessor;

    State state = REMOVAL_ALLOWED;

    // As an optimization we do minimization in quasi-sorted way:
    // first minimize nodes at least 1024 lines long, then 512, and so on
    unsigned linesUpperLimit = INT_MAX;
    unsigned linesLowerLimit = 1024;

    std::string rewrittenTypeInfo;

    /// Visit all child nodes
    template <typename T>
    void visitDefault(T&& node) {
        for (uint32_t i = 0; i < node.getChildCount(); i++) {
            auto child = node.childNode(i);
            if (child)
                child->visit(*DERIVED, node.isChildOptional(i));
        }
    }

    // Do some state bookkeeping, try to call type-specific handler, and visit node's children.
    // Visiting children can be disabled by returning DONT_VISIT_CHILDREN from handle()
    template <typename T>
    void visit(T&& node, bool isNodeRemovable = true) {
        if (state == SKIP_TO_START && node.sourceRange() == startPoint) {
            state = REMOVAL_ALLOWED;
        }

        if (state == REGISTER_CHILD && node.sourceRange() != SourceRange::NoLocation &&
            node.sourceRange() != rewritePoint) {  // avoid marking rewritten node as its own children
            rewritePointChildren = node.sourceRange();
            state = EXIT_REWRITE_POINT;
            return;
        }

        if (state == REGISTER_SUCCESSOR && node.sourceRange() != SourceRange::NoLocation) {
            rewritePointSuccessor = node.sourceRange();
            state = SKIP_TO_END;
            return;
        }

        if (state == SKIP_TO_END || state == EXIT_REWRITE_POINT) {
            return;
        }

        if constexpr (requires { DERIVED->handle(node, isNodeRemovable); }) {
            if (DERIVED->handle(node, isNodeRemovable) == VISIT_CHILDREN) {
                DERIVED->visitDefault(node);
            }
        } else {
            DERIVED->visitDefault(node);
        }

        if ((state == REGISTER_CHILD || state == EXIT_REWRITE_POINT) &&
            node.sourceRange() == rewritePoint) {
            state = REGISTER_SUCCESSOR;
        }
    }

    template <typename T>
    bool shouldRemove(const T& node, bool isNodeRemovable) {
        unsigned len = std::ranges::count(node.toString(), '\n') + 1;
        return state == REMOVAL_ALLOWED && isNodeRemovable && len >= linesLowerLimit &&
               len < linesUpperLimit;
    }

    template <typename T>
    bool shouldRemove(const SyntaxList<T>& list) {
        unsigned len = std::ranges::count(list.toString(), '\n') + 1;
        return state == REMOVAL_ALLOWED && list.getChildCount() && len >= linesLowerLimit &&
               len < linesUpperLimit;
    }

    template <typename T>
    void logType() {
        std::cerr << STRINGIZE_NODE_TYPE(T) << "\n";
        rewrittenTypeInfo = STRINGIZE_NODE_TYPE(T);
    }

    template <typename T>
    void removeNode(const T& node, bool isNodeRemovable) {
        if (shouldRemove(node, isNodeRemovable)) {
            logType<T>();
            std::cerr << node.toString() << "\n";
            DERIVED->remove(node);
            rewritePoint = node.sourceRange();
            state = REGISTER_CHILD;
        }
    }

    template <typename TParent, typename TChild>
    void removeChildList(const TParent& parent, const SyntaxList<TChild>& childList) {
        if (shouldRemove(childList)) {
            logType<TParent>();
            for (auto item : childList) {
                DERIVED->remove(*item);
                std::cerr << item->toString();
            }
            std::cerr << "\n";
            rewritePoint = parent.sourceRange();
            state = REGISTER_CHILD;  // TODO: examine whether we register right child here
        }
    }

    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree> tree,
                                          bool& traversalDone,
                                          AttemptStats& stats) {
        // Apply one rewrite, and return changed tree.
        // traversalDone is set when subsequent calls to transform would not make sense
        rewritePoint = SourceRange::NoLocation;
        rewritePointChildren = SourceRange::NoLocation;
        rewritePointSuccessor = SourceRange::NoLocation;

        auto tree2 = SyntaxRewriter<TDerived>::transform(tree);

        if (rewritePointChildren == SourceRange::NoLocation &&
            rewritePointSuccessor == SourceRange::NoLocation) {
            // we have ran out of nodes of searched size - advance limit
            linesUpperLimit = linesLowerLimit;
            linesLowerLimit /= 2;
            if (linesUpperLimit == 1) {  // tried all possible sizes - finish
                traversalDone = true;
            } else if (rewritePoint == SourceRange::NoLocation) {
                // no node rewritten - retry with new limit
                tree2 = transform(tree, traversalDone, stats);
            }
        }

        stats.typeInfo = rewrittenTypeInfo;
        return tree2;
    }

    void moveStartToSuccesor() {
        // Start next transform from successor of rewritten node.
        // Meant be run when you decided to commit removal (i.e you pass just transformed tree for
        // next transform)
        startPoint = rewritePointSuccessor;
        state = SKIP_TO_START;
    }

    void moveStartToChildOrSuccesor() {
        // Start next transform from child of rewritten node if possible, otherwise, from its
        // successor. Meant to be run when you decide to rollback removal (i.e. you're discarding a
        // just transformed tree)
        if (rewritePointChildren != SourceRange::NoLocation) {
            startPoint = rewritePointChildren;
        } else {
            startPoint = rewritePointSuccessor;
        }
        state = SKIP_TO_START;
    }
};

template <typename T>
bool rewriteLoop(std::shared_ptr<SyntaxTree>& tree, std::string stageName, std::string passIdx) {
    T rewriter;
    bool committed = false;
    bool traversalDone = false;

    while (!traversalDone) {
        auto stats = AttemptStats(passIdx, stageName);
        ;
        auto tmpTree = rewriter.transform(tree, traversalDone, stats);
        if (traversalDone && tmpTree == tree) {
            break;  // no change - no reason to test
        }
        if (test(tmpTree, stats)) {
            tree = tmpTree;
            rewriter.moveStartToSuccesor();
            committed = true;
        } else {
            rewriter.moveStartToChildOrSuccesor();
        }
    }
    return committed;
}
