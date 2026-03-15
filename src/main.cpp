#include "capybara/ast.h"
#include "capybara/compiler.h"
#include "capybara/lexer.h"
#include "capybara/lsp.h"
#include "capybara/parser.h"
#include "capybara/token.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::Optional);

static cl::opt<bool> DumpTokens("dump-tokens",
                                cl::desc("Dump the token stream and exit"),
                                cl::init(false));

static cl::opt<bool> Lsp("lsp", cl::desc("Enable LSP mode"), cl::init(false));

static cl::opt<bool> DumpAST("dump-ast", cl::desc("Dump the AST and exit"),
                             cl::init(false));

static cl::opt<bool> Verbose("verbose", cl::desc("Enable verbose output"),
                             cl::init(false));

static cl::opt<std::string>
    TargetTriple("target", cl::desc("Target triple for cross-compilation"),
                 cl::init(""));

static cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"),
                                           cl::value_desc("file"),
                                           cl::init(""));

static cl::opt<bool> NoStdLib("no-stdlib",
                              cl::desc("Do not import standard library"),
                              cl::init(false));

static void dumpTokens(llvm::StringRef source, llvm::StringRef filename) {
  capybara::Lexer lexer(source, filename);
  auto tokens = lexer.lexAll();

  llvm::outs() << "=== Token Stream (" << filename << ") ===\n";
  for (const auto &tok : tokens) {
    llvm::outs() << llvm::formatv("{0,4}:{1,-3}  {2,-12}", tok.location.line,
                                  tok.location.column,
                                  capybara::Token::kindName(tok.kind));
    if (tok.kind != capybara::TokenKind::Newline &&
        tok.kind != capybara::TokenKind::Indent &&
        tok.kind != capybara::TokenKind::Dedent &&
        tok.kind != capybara::TokenKind::Eof && !tok.value.empty()) {
      llvm::outs() << "  '" << tok.value << "'";
    }
    llvm::outs() << "\n";
  }

  if (lexer.hasErrors()) {
    llvm::errs() << lexer.getErrorCount() << " lexer error(s)\n";
  }
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  cl::SetVersionPrinter([](raw_ostream &OS) {
    OS << "capybara 0.1.0\n";
    OS << "A compiled language with Python-compatible syntax\n";
    OS << "Built with LLVM " << LLVM_VERSION_STRING << "\n";
  });

  cl::ParseCommandLineOptions(
      argc, argv,
      "Capybara - a compiled language with Mojo/Python-compatible syntax\n\n"
      "USAGE:\n"
      "  capybara [options] <input.py>\n\n"
      "EXAMPLES:\n"
      "  capybara hello.py              # Compile\n"
      "  capybara --dump-tokens hello.py # Show tokens\n"
      "  capybara --dump-ast hello.py    # Show AST\n");

  if (Lsp) {
    // if (!llvm::sys::ChangeStdinToBinary()) {
    //   WithColor::error() << "Failed to change stdin to binary mode\n";
    //   return 1;
    // }

    capybara::LanguageServer server(stdin, llvm::outs());
    server.run();

    return 0;
  }

  auto fileOrErr = MemoryBuffer::getFile(InputFilename);
  if (auto ec = fileOrErr.getError()) {
    WithColor::error() << "could not open '" << InputFilename
                       << "': " << ec.message() << "\n";
    return 1;
  }

  auto &buffer = *fileOrErr;
  StringRef source = buffer.get()->getBuffer();
  StringRef filename = InputFilename;

  if (Verbose) {
    llvm::outs() << "capybara: processing " << filename << " (" << source.size()
                 << " bytes)\n";
  }

  if (DumpTokens) {
    dumpTokens(source, filename);
    return 0;
  }

  capybara::Lexer lexer(source, filename);
  capybara::Parser parser(lexer);

  auto ast = parser.parseModule();

  if (lexer.hasErrors() || parser.hasErrors()) {
    unsigned total = lexer.getErrorCount() + parser.getErrorCount();
    WithColor::error() << total << " error(s) generated\n";
    return 1;
  }

  if (DumpAST) {
    llvm::outs() << "=== AST ===\n";
    ast->dump();
    return 0;
  }

  if (Verbose) {
    llvm::outs() << "capybara: parsed " << ast->declarations.size()
                 << " top-level declaration(s)\n";
  }

  capybara::Compiler compiler(std::move(ast));
  if (!TargetTriple.empty()) {
    compiler.setTargetTriple(TargetTriple);
  }
  if (!NoStdLib) {
    compiler.enableStdLib();
  }
  return compiler.compile();
}
