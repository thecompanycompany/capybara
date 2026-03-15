#pragma once

#include "capybara/token.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include <memory>
#include <string>
#include <vector>

namespace capybara {

/// Lexer for the Capybara language.
///
/// Handles indentation-based scoping (emitting INDENT/DEDENT tokens),
/// all Mojo/Python-inspired keywords, operators, and literals.
class Lexer {
public:
  explicit Lexer(llvm::StringRef source, llvm::StringRef filename = "<input>");

  /// Lex the entire source into a token stream.
  std::vector<Token> lexAll();

  /// Lex the next token.
  Token nextToken();

  /// Peek at the next token without consuming it.
  const Token &peek();

  /// Get the filename being lexed.
  llvm::StringRef getFilename() const { return filename_; }

  /// Check if there have been any lexer errors.
  bool hasErrors() const { return errorCount_ > 0; }
  unsigned getErrorCount() const { return errorCount_; }

private:
  llvm::StringRef source_;
  llvm::StringRef filename_;
  const char *current_;
  const char *end_;
  uint32_t line_ = 1;
  uint32_t column_ = 1;
  unsigned errorCount_ = 0;

  // Indentation tracking
  std::vector<unsigned> indentStack_ = {0};
  int pendingDedents_ = 0;
  bool atLineStart_ = true;
  bool pendingNewline_ = false;

  // Peeked token
  bool hasPeeked_ = false;
  Token peekedToken_;

  // Character-level helpers
  char peekChar() const;
  char peekChar(int offset) const;
  char advance();
  bool isAtEnd() const;
  bool match(char expected);
  void skipLineComment();
  void skipWhitespaceInLine();

  // Token production
  Token makeToken(TokenKind kind, const std::string &value, SourceLocation loc);
  Token makeError(const std::string &message, SourceLocation loc);
  SourceLocation currentLocation() const;

  // Lexing routines
  Token lexIndentation();
  Token lexNumber();
  Token lexString();
  Token lexIdentifierOrKeyword();
  Token lexOperatorOrDelimiter();
  Token lexTokenInternal();

  // Keyword lookup
  static TokenKind lookupKeyword(llvm::StringRef word);
};

} // namespace capybara
