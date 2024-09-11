#pragma once
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <iostream>
#include <string>

using namespace slang::ast;
using namespace slang::syntax;
using namespace slang;

std::string prettifyNodeTypename(const char* type);
// stringize type of node, demangle and remove namespace specifier
#define STRINGIZE_NODE_TYPE(TYPE) prettifyNodeTypename(typeid(TYPE).name())

void printSyntaxTree(const std::shared_ptr<SyntaxTree>& tree, std::ostream& file);
void printAst(const RootSymbol& tree, std::ostream& file);
std::string toString(SourceRange sourceRange);
