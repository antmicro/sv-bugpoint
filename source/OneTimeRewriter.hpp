// SPDX-License-Identifier: Apache-2.0
#include <slang/syntax/SyntaxVisitor.h>
#include <iosfwd>
#include "SvBugpoint.hpp"
#include "Utils.hpp"
#include "slang/text/SourceLocation.h"

#define DERIVED static_cast<TDerived*>(this)

struct CheckPoint {
    SourceRange rewritePoint;
    SourceRange childOrSibling;
    SourceRange sibling;
    CheckPoint(SourceRange rewritePoint)
        : rewritePoint(rewritePoint),
          childOrSibling(SourceRange::NoLocation),
          sibling(SourceRange::NoLocation) {}
};

template <typename TDerived>
class OneTimeRewriter : public SyntaxRewriter<TDerived> {
    // Incremental node rewriter - each transform() yields n rewrites at most
   public:
    enum State {
        SKIP_TO_START,
        REWRITE_ALLOWED,
        REGISTER_CHILD,
        EXIT_REWRITE_POINT,
        REGISTER_SUCCESSOR,
        SKIP_TO_END,
    };

    enum ShouldVisitChildren {
        VISIT_CHILDREN,
        DONT_VISIT_CHILDREN,
    };

    SourceRange startPoint = SourceRange::NoLocation;

    std::vector<CheckPoint> checkPoints;

    size_t rewriteLimit;

    State state = REWRITE_ALLOWED;

    // As an optimization we do minimization in quasi-sorted way:
    // first minimize nodes at least 1024 lines long, then 512, and so on
    unsigned linesUpperLimit = INT_MAX;
    unsigned linesLowerLimit = 1024;

    bool traversalDone = false;  // set once all line "windows" were tried

    std::string rewrittenTypeInfo;

    /// Visit all child nodes
    template <typename T>
    void visitDefault(T&& node) {
        for (uint32_t i = 0; i < node.getChildCount(); i++) {
            auto child = node.childNode(i);
            if (child) {
                child->visit(*DERIVED, node.isChildOptional(i));
            }
        }
        if (node.getChildCount() == 0) {  // leaf node
            node.visit(*DERIVED, true);
        }
    }

    // Do some state bookkeeping, try to call type-specific handler, and visit node's children.
    // Visiting children can be disabled by returning DONT_VISIT_CHILDREN from handle()
    template <typename T>
    void visit(T&& node, bool isNodeRemovable = true) {
        if (state == SKIP_TO_START && node.sourceRange() == startPoint) {
            state = REWRITE_ALLOWED;
        }

        if (state == REGISTER_CHILD && node.sourceRange() != SourceRange::NoLocation &&
            node.sourceRange() !=
                checkPoints.back()
                    .rewritePoint) {  // avoid marking rewritten node as its own children
            checkPoints.back().childOrSibling = node.sourceRange();
            state = EXIT_REWRITE_POINT;
            return;
        }

        if (state == REGISTER_SUCCESSOR && node.sourceRange() != SourceRange::NoLocation) {
            checkPoints.back().sibling = node.sourceRange();
            if (checkPoints.back().childOrSibling == SourceRange::NoLocation) {
                checkPoints.back().childOrSibling = node.sourceRange();
            }

            if (checkPoints.size() < rewriteLimit) {
                state = REWRITE_ALLOWED;
            } else {
                state = SKIP_TO_END;
            }
        }

        if (state == SKIP_TO_END || state == EXIT_REWRITE_POINT) {
            return;
        }

        if constexpr (requires { DERIVED->handle(node, isNodeRemovable); }) {
            if (DERIVED->handle(node, isNodeRemovable) == VISIT_CHILDREN) {
                DERIVED->visitDefault(node);
            }
        } else if (node.getChildCount() > 0) {
            DERIVED->visitDefault(node);
        }

        if ((state == REGISTER_CHILD || state == EXIT_REWRITE_POINT) &&
            node.sourceRange() == checkPoints.back().rewritePoint) {
            state = REGISTER_SUCCESSOR;
        }
    }

    template <typename T>
    bool shouldRemove(const T& node, bool isNodeRemovable) {
        unsigned len = std::ranges::count(node.toString(), '\n') + 1;
        return state == REWRITE_ALLOWED && isNodeRemovable && len >= linesLowerLimit &&
               len < linesUpperLimit;
    }

    template <typename T>
    bool shouldRemove(const SyntaxList<T>& list) {
        unsigned len = std::ranges::count(list.toString(), '\n') + 1;
        return state == REWRITE_ALLOWED && list.getChildCount() && len >= linesLowerLimit &&
               len < linesUpperLimit;
    }

    template <typename T>
    bool shouldReplace(const T& node) {
        unsigned len = std::ranges::count(node.toString(), '\n') + 1;
        return state == REWRITE_ALLOWED && len >= linesLowerLimit && len < linesUpperLimit;
    }

    template <typename T>
    void logType() {
        std::cerr << STRINGIZE_NODE_TYPE(T) << "\n";
        if (!rewrittenTypeInfo.empty()) {
            rewrittenTypeInfo += ",";
        }
        rewrittenTypeInfo += STRINGIZE_NODE_TYPE(T);
    }

    template <typename T>
    void removeNode(const T& node, bool isNodeRemovable) {
        if (shouldRemove(node, isNodeRemovable)) {
            logType<T>();
            std::cerr << prefixLines(node.toString(), "-") << "\n";
            DERIVED->remove(node);
            checkPoints.push_back({node.sourceRange()});
            state = REGISTER_CHILD;
        }
    }

    template <typename TParent, typename TChild>
    void removeChildList(const TParent& parent, const SyntaxList<TChild>& childList) {
        if (shouldRemove(childList)) {
            logType<TParent>();
            for (auto item : childList) {
                DERIVED->remove(*item);
                std::cerr << prefixLines(item->toString(), "-");
            }
            std::cerr << "\n";
            checkPoints.push_back({parent.sourceRange()});
            state = REGISTER_CHILD;
        }
    }

    template <typename TOrig, typename TNew>
    void replaceNode(const TOrig& originalNode, TNew& newNode) {
        if (shouldReplace(originalNode)) {
            logType<TOrig>();
            std::cerr << prefixLines(originalNode.toString(), "-") << "\n";
            std::cerr << prefixLines(newNode.toString(), "+") << "\n";
            DERIVED->replace(originalNode, newNode);
            checkPoints.push_back({originalNode.sourceRange()});
            state = REGISTER_CHILD;
        }
    }

    bool advanceLineWindow() {
        if (linesLowerLimit == 1) {  // tried all possible sizes - finish
            traversalDone = true;
            return false;
        } else {
            state = REWRITE_ALLOWED;
            startPoint = SourceRange::NoLocation;
            linesUpperLimit = linesLowerLimit;
            linesLowerLimit = linesLowerLimit / 2;
            return true;
        }
    }

    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree> tree,
                                          AttemptStats& stats,
                                          int n = 1) {
        // Apply n rewrites, and return changed tree.
        checkPoints.clear();
        rewriteLimit = n;
        rewrittenTypeInfo = "";

        auto tree2 = SyntaxRewriter<TDerived>::transform(tree);

        if (checkPoints.size() == 0 && advanceLineWindow()) {
            // we have run out of nodes of searched size - retry with new line window if possible
            return transform(tree, stats, n);
        }

        stats.typeInfo = rewrittenTypeInfo;
        return tree2;
    }

    void moveToPoint(SourceRange point) {
        // Start next transform from suplied node
        startPoint = point;
        if (startPoint == SourceRange::NoLocation) {
            advanceLineWindow();
        } else {
            state = SKIP_TO_START;
        }
    }

    void retry() {
        // Start next transform from first rewritten node.
        // Meant to be run when you decide to rollback removal (i.e. you're discarding a
        // just transformed tree), and retry it subset
        if (startPoint == SourceRange::NoLocation) {
            state = REWRITE_ALLOWED;
        } else {
            state = SKIP_TO_START;
        }
    }
};

enum class RewriteResult {
    PASS,
    FAIL,
    NONE,
};

template <typename T>
RewriteResult rewrite(T& rewriter,
                      std::shared_ptr<SyntaxTree>& tree,
                      std::string stageName,
                      std::string passIdx,
                      SvBugpoint* svBugpoint,
                      int n) {
    auto stats = AttemptStats(passIdx, stageName, svBugpoint);
    auto tmpTree = rewriter.transform(tree, stats, n);

    if (rewriter.traversalDone && tmpTree == tree) {
        return RewriteResult::NONE;  // no change - no reason to test
    }

    if (svBugpoint->test(tmpTree, stats)) {
        tree = tmpTree;
        return RewriteResult::PASS;
    } else {
        return RewriteResult::FAIL;
    }
}

template <typename T>
size_t rewriteBisectFailed(T& rewriter,
                           std::shared_ptr<SyntaxTree>& tree,
                           std::string stageName,
                           std::string passIdx,
                           SvBugpoint* svBugpoint,
                           size_t n) {
    // like rewriteBisect, but assume that removing all n nodes would fail
    if (n == 0 || n == 1) {
        return 0;
    }

    using enum RewriteResult;
    size_t rewritten = rewriteBisect(rewriter, tree, stageName, passIdx, svBugpoint, n / 2, false);
    if (rewritten < n / 2) {
        // The culprit was in [0, n/2)
        return rewritten;
    } else {
        // The culprit is in remaining nodes
        size_t remainingNodes = n - rewritten;
        return rewritten +
               rewriteBisectFailed(rewriter, tree, stageName, passIdx, svBugpoint, remainingNodes);
    }
}

template <typename T>
size_t rewriteBisect(T& rewriter,
                     std::shared_ptr<SyntaxTree>& tree,
                     std::string stageName,
                     std::string passIdx,
                     SvBugpoint* svBugpoint,
                     size_t n,
                     bool topMostCall = true) {
    RewriteResult result = rewrite(rewriter, tree, stageName, passIdx, svBugpoint, n);

    if (result == RewriteResult::PASS) {
        rewriter.moveToPoint(rewriter.checkPoints.back().sibling);
        return rewriter.checkPoints.size();
    } else if (result == RewriteResult::FAIL) {
        rewriter.retry();
        if (topMostCall) {
            auto checkPoints = rewriter.checkPoints;
            size_t rewritten = rewriteBisectFailed(rewriter, tree, stageName, passIdx, svBugpoint,
                                                   checkPoints.size());
            // The culprit sits after the last succesfully rewritten node.
            // Next attempts should start from its children
            rewriter.moveToPoint(checkPoints[rewritten].childOrSibling);
            return rewritten;
        } else {
            return rewriteBisectFailed(rewriter, tree, stageName, passIdx, svBugpoint,
                                       rewriter.checkPoints.size());
        }
    } else {
        assert(result == RewriteResult::NONE && topMostCall);
        return 0;
    }
}

template <typename T>
bool rewriteLoop(std::shared_ptr<SyntaxTree>& tree,
                 std::string stageName,
                 std::string passIdx,
                 SvBugpoint* svBugpoint) {
    using enum RewriteResult;
    T rewriter;
    bool committed = false;
    while (!rewriter.traversalDone) {
        if (rewriteBisect(rewriter, tree, stageName, passIdx, svBugpoint, svBugpoint->n_at_once)) {
            committed = true;
        }
    }
    return committed;
}
