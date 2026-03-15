#pragma once

#include "llvm/Support/raw_ostream.h"
#include <capybara/ast.h>
#include <llvm/Support/LSP/Logging.h>
#include <llvm/Support/LSP/Protocol.h>
#include <llvm/Support/LSP/Transport.h>

namespace capybara {

class LanguageServer : public std::enable_shared_from_this<LanguageServer> {
public:
  LanguageServer(std::FILE *in, llvm::raw_ostream &out);

  void run();

private:
  llvm::lsp::JSONTransport transport_;

  void onInitialize(const llvm::lsp::InitializeParams &params,
                    llvm::lsp::Callback<llvm::json::Value> reply);
};

} // namespace capybara
