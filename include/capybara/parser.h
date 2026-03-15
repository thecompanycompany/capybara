#pragma once

#include "capybara/ast.h"
#include "capybara/lexer.h"
#include "capybara/token.h"
#include <memory>
#include <vector>

namespace capybara {

/// Recursive descent parser for Capybara.
///
/// Consumes tokens from the Lexer and builds an AST.
/// Handles indentation-based blocks via INDENT/DEDENT tokens.
class Parser {
public:
  explicit Parser(Lexer &lexer);

  /// Parse an entire module (file).
  std::unique_ptr<Module> parseModule();

  bool hasErrors() const { return errorCount_ > 0; }
  unsigned getErrorCount() const { return errorCount_; }

private:
  Lexer &lexer_;
  Token current_;
  Token previous_;
  unsigned errorCount_ = 0;
  bool panicMode_ = false;

  // Token helpers
  Token advance();
  bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  Token expect(TokenKind kind, const char *message);
  void synchronize();

  template <typename... Ts> bool checkOneOf(Ts... kinds) const {
    return (check(kinds) || ...);
  }

  // Error reporting
  void error(const char *message);
  void error(const Token &token, const char *message);
  void errorAt(const SourceLocation &loc, const char *message);

  // Skip newlines between statements (but not INDENT/DEDENT)
  void skipNewlines();

  // Expect a newline or EOF after a statement
  void expectNewline();

  // Parse routines
  StmtPtr parseDeclaration();
  StmtPtr parseStatement();

  // Declarations
  std::unique_ptr<DefDecl> parseDefDecl();
  std::unique_ptr<ClassDecl> parseClassDecl();
  std::unique_ptr<StructDecl> parseStructDecl();
  std::unique_ptr<AliasDecl> parseAliasDecl();
  std::unique_ptr<VariableDeclaration> parseMojoVarDecl();
  std::unique_ptr<ImportStmt> parseImportStmt();
  std::unique_ptr<AsmStmt> parseAsmStmt();

  // Statements
  std::unique_ptr<ReturnStatement> parseReturnStmt();
  std::unique_ptr<IfStatement> parseIfStmt();
  std::unique_ptr<WhileStatement> parseWhileStmt();
  std::unique_ptr<ForStatement> parseForStmt();
  std::unique_ptr<TryExceptStmt> parseTryExceptStmt();
  std::unique_ptr<RaiseStmt> parseRaiseStmt();
  StmtPtr parseExprStmt();

  // Blocks (indentation-based)
  std::unique_ptr<Block> parseBlock();

  // Function signature helpers
  std::vector<Parameter> parseParamList();
  TypePointer parseType();

  // Expression parsing (Pratt-style precedence climbing)
  ExpressionPointer parseExpression();
  ExpressionPointer parseAssignment();
  ExpressionPointer parseOr();
  ExpressionPointer parseAnd();
  ExpressionPointer parseEquality();
  ExpressionPointer parseComparison();
  ExpressionPointer parseBitwiseOr();
  ExpressionPointer parseBitwiseXor();
  ExpressionPointer parseBitwiseAnd();
  ExpressionPointer parseTerm();
  ExpressionPointer parseFactor();
  ExpressionPointer parsePower();
  ExpressionPointer parseUnary();
  ExpressionPointer parsePostfix();
  ExpressionPointer parsePrimary();

  // Postfix helpers
  ExpressionPointer parseCallArgs(ExpressionPointer callee);
  ExpressionPointer parseMemberAccess(ExpressionPointer object);
  ExpressionPointer parseIndexAccess(ExpressionPointer object);
};

} // namespace capybara
