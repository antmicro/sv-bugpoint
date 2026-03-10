// SPDX-License-Identifier: Apache-2.0
#include <slang/syntax/SyntaxTree.h>
#include "SvBugpoint.hpp"

using namespace slang::syntax;

// forward declarations of simple code removers that try to remove nodes one-by-one
class BodyPartsRemover;
class BodyRemover;
class LabelRemover;
class DeclRemover;
class StatementsRemover;
class ImportsRemover;
class MemberRemover;
class ParamAssignRemover;
class ContAssignRemover;
class ModportRemover;
class InstantationRemover;
class BindRemover;
class ModuleRemover;
class TypeSimplifier;

template <typename T>
bool rewriteLoop(std::shared_ptr<SyntaxTree>& tree,
                 std::string stageName,
                 std::string passIdx,
                 SvBugpoint* svBugpoint);
