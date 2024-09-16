#include <slang/syntax/SyntaxVisitor.h>
#include "utils.hpp"

#define DERIVED static_cast<TDerived*>(this)

template <typename TDerived>
class OneTimeRemover : public SyntaxRewriter<TDerived> {
    // Incremental node remover - each transform() yields one removal at most
   public:
    enum State {
        SKIP_TO_START,
        REMOVAL_ALLOWED,
        REGISTER_CHILD,
        WAIT_FOR_PARENT_EXIT,
        REGISTER_SUCCESSOR,
        SKIP_TO_END,
    };

    enum ShouldVisitChildren {
        VISIT_CHILDREN,
        DONT_VISIT_CHILDREN,
    };

    SourceRange startPoint;
    SourceRange removed;
    SourceRange removedChild;
    SourceRange removedSuccessor;

    State state = REMOVAL_ALLOWED;

    // As an optimization we do removal in quasi-sorted way:
    // first remove nodes at least 1024 lines long, then 512, and so on
    unsigned linesUpperLimit = INT_MAX;
    unsigned linesLowerLimit = 1024;

    std::string removedTypeInfo;

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
            node.sourceRange() != removed) {  // avoid marking removed node as its own children
            removedChild = node.sourceRange();
            state = WAIT_FOR_PARENT_EXIT;
            return;
        }

        if (state == REGISTER_SUCCESSOR && node.sourceRange() != SourceRange::NoLocation) {
            removedSuccessor = node.sourceRange();
            state = SKIP_TO_END;
            return;
        }

        if (state == SKIP_TO_END || state == WAIT_FOR_PARENT_EXIT) {
            return;
        }

        if constexpr (requires { DERIVED->handle(node, isNodeRemovable); }) {
            if (DERIVED->handle(node, isNodeRemovable) == VISIT_CHILDREN) {
                DERIVED->visitDefault(node);
            }
        } else {
            DERIVED->visitDefault(node);
        }

        if ((state == REGISTER_CHILD || state == WAIT_FOR_PARENT_EXIT) &&
            node.sourceRange() == removed) {
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
        removedTypeInfo = STRINGIZE_NODE_TYPE(T);
    }

    template <typename T>
    void removeNode(const T& node, bool isNodeRemovable) {
        if (shouldRemove(node, isNodeRemovable)) {
            logType<T>();
            std::cerr << node.toString() << "\n";
            DERIVED->remove(node);
            removed = node.sourceRange();
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
            removed = parent.sourceRange();
            state = REGISTER_CHILD;  // TODO: examine whether we register right child here
        }
    }

    std::shared_ptr<SyntaxTree> transform(const std::shared_ptr<SyntaxTree> tree,
                                          bool& traversalDone,
                                          AttemptStats& stats) {
        // Apply one removal, and return changed tree.
        // traversalDone is set when subsequent calls to transform would not make sense
        removed = SourceRange::NoLocation;
        removedChild = SourceRange::NoLocation;
        removedSuccessor = SourceRange::NoLocation;

        auto tree2 = SyntaxRewriter<TDerived>::transform(tree);

        if (removedChild == SourceRange::NoLocation &&
            removedSuccessor == SourceRange::NoLocation) {
            // we have ran out of nodes of searched size - advance limit
            linesUpperLimit = linesLowerLimit;
            linesLowerLimit /= 2;
            if (linesUpperLimit == 1) {  // tried all possible sizes - finish
                traversalDone = true;
            } else if (removed == SourceRange::NoLocation) {
                // no node removed - retry with new limit
                tree2 = transform(tree, traversalDone, stats);
            }
        }

        stats.typeInfo = removedTypeInfo;
        return tree2;
    }

    void moveStartToSuccesor() {
        // Start next transform from successor of removed node.
        // Meant be run when you decided to commit removal (i.e you pass just transformed tree for
        // next transform)
        startPoint = removedSuccessor;
        state = SKIP_TO_START;
    }

    void moveStartToChildOrSuccesor() {
        // Start next transform from child of removed node if possible, otherwise, from its
        // successor. Meant to be run when you decide to rollback removal (i.e. you're discarding a
        // just transformed tree)
        if (removedChild != SourceRange::NoLocation) {
            startPoint = removedChild;
        } else {
            startPoint = removedSuccessor;
        }
        state = SKIP_TO_START;
    }
};

template <typename T>
bool removeLoop(std::shared_ptr<SyntaxTree>& tree, std::string stageName, std::string passIdx) {
    T remover;
    bool committed = false;
    bool traversalDone = false;

    while (!traversalDone) {
        auto stats = AttemptStats(passIdx, stageName);
        ;
        auto tmpTree = remover.transform(tree, traversalDone, stats);
        if (traversalDone && tmpTree == tree) {
            break;  // no change - no reason to test
        }
        if (test(tmpTree, stats)) {
            tree = tmpTree;
            remover.moveStartToSuccesor();
            committed = true;
        } else {
            remover.moveStartToChildOrSuccesor();
        }
    }
    return committed;
}
