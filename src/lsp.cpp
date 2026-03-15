#include "llvm/Support/raw_ostream.h"
#include <capybara/lsp.h>

namespace capybara {
LanguageServer::LanguageServer(std::FILE *in, llvm::raw_ostream &out)
    : transport_(in, out, llvm::lsp::JSONStreamStyle::Standard, false) {}

void LanguageServer::run() {
  llvm::lsp::MessageHandler messageHandler(transport_);
  messageHandler.method("initialize", this, &LanguageServer::onInitialize);
}

void LanguageServer::onInitialize(
    const llvm::lsp::InitializeParams &params,
    llvm::lsp::Callback<llvm::json::Value> reply) {
  llvm::json::Object serverCaps{
      {"textDocumentSync",
       llvm::json::Object{
           {"openClose", true},
           {"change", (int)llvm::lsp::TextDocumentSyncKind::Incremental},
           {"save", true},
       }},
      {"completionProvider",
       llvm::json::Object{
           {"allCommitCharacters",
            {"\t", "(", ")", "[", "]", "{",  "}", "<", ">",
             ":",  ";", ",", "+", "-", "/",  "*", "%", "^",
             "&",  "#", "?", ".", "=", "\"", "'", "|"}},
           {"resolveProvider", false},
           {"triggerCharacters",
            {".", ">", "(", "{", ",", "<", ":", "[", " ", "\"", "/"}},
       }},
      {"signatureHelpProvider",
       llvm::json::Object{
           {"triggerCharacters", {"(", ","}},
       }},
      {"definitionProvider", true},
      {"referencesProvider", true},
      {"documentLinkProvider",
       llvm::json::Object{
           {"resolveProvider", false},
       }},
      {"hoverProvider", true},
      {"documentSymbolProvider", true},
      {"inlayHintProvider", true},
  };

  llvm::json::Object result{
      {{"serverInfo", llvm::json::Object{{"name", "mlir-pdll-lsp-server"},
                                         {"version", "0.0.1"}}},
       {"capabilities", std::move(serverCaps)}}};
  reply(std::move(result));
}

} // namespace capybara
