#include <slang/syntax/SyntaxPrinter.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include "slang/syntax/AllSyntax.h"

using namespace slang::syntax;
using namespace slang::ast;
using namespace slang;

class Obfuscator : public SyntaxVisitor<Obfuscator> {
   public:
    std::string obfuscate(std::shared_ptr<SyntaxTree>& tree) {
        BumpAllocator alloc_;
        SyntaxPrinter printer_ =
            SyntaxPrinter(tree->sourceManager())
                // Sane defaults like we would get from SyntaxPrinter::printFile()
                .setIncludeDirectives(true)
                .setIncludeSkipped(true)
                .setIncludeTrivia(true)
                .setSquashNewlines(false);
        alloc = &alloc_;
        printer = &printer_;
        visit(tree->root());
        return printer->str();
    }

    void visitToken(parsing::Token tok) {
        if (tok.kind == slang::parsing::TokenKind::Identifier) {
            tok = tok.withRawText(*alloc, translate(tok));
        }
        printer->print(tok);
    }

    std::string translate(parsing::Token tok) {
        std::string name = std::string(tok.valueText());
        if (translationMap.contains(name)) {
            return translationMap[name];
        } else {
            std::string obfuscated = "id" + std::to_string(counter++);
            translationMap[name] = obfuscated;
            return obfuscated;
        }
    }

    // Shared between obfuscate calls, so multi-source inputs get consistent ids
    std::unordered_map<std::string, std::string> translationMap;
    int counter = 0;

    // Created for each obfuscate call
    SyntaxPrinter* printer;
    BumpAllocator* alloc;
};

std::string getOutPath(const char* outDir, const char* in, size_t idx) {
    std::string extension = std::filesystem::path(in).extension().string();
    return std::string(outDir) + "/" + std::to_string(idx) + extension;
}

int main(int argc, char** argv) {
    if (argc < 3 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        std::cerr << "Utility for obfuscating SystemVerilog source files\n";
        std::cerr << "\n";
        std::cerr << "Usage: sv-obfuscate output_dir input_files...\n";
        std::cerr << "\n";
        std::cerr << "Renames identifiers to idN format (where N is number assigned in order of "
                     "appearance).\n";
        std::cerr << "\n";
        std::cerr << "Obfuscated code is written into output_dir, and a translation map is printed "
                     "to stdout.\n";
        std::cerr << "\n";
        std::cerr << "Limitations:\n";
        std::cerr << "- Macros are not obfuscated\n";
        std::cerr << "- Non-standard syntax extensions (such as `verilator_config block) may be "
                     "misrecognized as identifiers\n";
        exit(0);
    }

    try {
        std::filesystem::create_directories(argv[1]);
    } catch (const std::filesystem::filesystem_error& err) {
        std::cerr << "failed to make directory '" << argv[1] << "': " << err.code().message()
                  << "\n";
        exit(1);
    }

    Obfuscator obfuscator;

    for (size_t i = 2; i < argc; i++) {
        size_t fileIdx = i - 1;
        std::string inPath = argv[i];
        std::string outPath = getOutPath(argv[1], argv[i], fileIdx);
        auto treeOrErr = SyntaxTree::fromFile(inPath);
        if (treeOrErr) {
            auto tree = *treeOrErr;
            std::ofstream out(outPath);
            out << obfuscator.obfuscate(tree);
        } else {
            std::cerr << "sv-obfuscate: failed to load '" << inPath
                      << "' file: " << std::string(treeOrErr.error().second) << "\n";
            exit(1);
        }
    }

    std::cout << "FILE_MAP\n";
    for (size_t i = 2; i < argc; i++) {
        size_t fileIdx = i - 1;
        std::cout << argv[i] << ":" << getOutPath(argv[1], argv[i], fileIdx) << "\n";
    }
    std::cout << "\nID_MAP\n";
    for (auto& it : obfuscator.translationMap) {
        std::cout << it.first << ":" << it.second << "\n";
    }
}
