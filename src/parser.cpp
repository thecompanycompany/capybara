#include "capybara/parser.h"
#include "llvm/Support/raw_ostream.h"
#include <charconv>

namespace capybara {

Parser::Parser(Lexer &lexer) : lexer_(lexer) {
  advance(); // prime the first token
}

// ── Token helpers ─────────────────────────────────────────────

Token Parser::advance() {
  previous_ = current_;
  for (;;) {
    current_ = lexer_.nextToken();
    if (current_.kind != TokenKind::Error)
      break;
  }
  return previous_;
}

bool Parser::check(TokenKind kind) const { return current_.kind == kind; }

bool Parser::match(TokenKind kind) {
  if (!check(kind))
    return false;
  advance();
  return true;
}

Token Parser::expect(TokenKind kind, const char *message) {
  if (check(kind))
    return advance();
  error(message);
  return current_;
}

void Parser::error(const char *message) { error(current_, message); }

void Parser::error(const Token &token, const char *message) {
  errorAt(token.location, message);
}

void Parser::errorAt(const SourceLocation &loc, const char *message) {
  if (panicMode_)
    return;
  panicMode_ = true;
  errorCount_++;
  llvm::errs() << lexer_.getFilename() << ":" << loc.line << ":" << loc.column
               << ": error: " << message;
  if (current_.kind == TokenKind::Eof) {
    llvm::errs() << " at end of file";
  } else if (current_.kind != TokenKind::Error) {
    llvm::errs() << " (got '" << current_.value << "')";
  }
  llvm::errs() << "\n";
}

void Parser::synchronize() {
  panicMode_ = false;
  while (current_.kind != TokenKind::Eof) {
    if (previous_.kind == TokenKind::Newline)
      return;
    switch (current_.kind) {
    case TokenKind::KwDef:
    case TokenKind::KwReturn:
    case TokenKind::KwIf:
    case TokenKind::KwWhile:
    case TokenKind::KwFor:
    case TokenKind::KwClass:
    case TokenKind::KwImport:
    case TokenKind::KwFrom:
      return;
    default:
      advance();
    }
  }
}

void Parser::skipNewlines() {
  while (match(TokenKind::Newline)) {
  }
}

void Parser::expectNewline() {
  if (check(TokenKind::Newline) || check(TokenKind::Eof) ||
      check(TokenKind::Dedent)) {
    if (check(TokenKind::Newline))
      advance();
    return;
  }
  error("expected newline after statement");
}

// ── Module ────────────────────────────────────────────────────

std::unique_ptr<Module> Parser::parseModule() {
  auto module = std::make_unique<Module>();
  module->filename = std::string(lexer_.getFilename());

  skipNewlines();

  while (!check(TokenKind::Eof)) {
    auto decl = parseDeclaration();
    if (decl) {
      module->declarations.push_back(std::move(decl));
    }
    skipNewlines();
  }

  return module;
}

// ── Declarations ──────────────────────────────────────────────

StmtPtr Parser::parseDeclaration() {
  skipNewlines();

  if (check(TokenKind::Eof))
    return nullptr;

  StmtPtr result;
  if (check(TokenKind::KwDef) || check(TokenKind::KwFn)) {
    result = parseDefDecl();
  } else if (check(TokenKind::KwClass)) {
    result = parseClassDecl();
  } else if (check(TokenKind::KwStruct)) {
    result = parseStructDecl();
  } else if (check(TokenKind::KwAlias)) {
    result = parseAliasDecl();
  } else if (check(TokenKind::KwVar) || check(TokenKind::KwLet)) {
    result = parseMojoVarDecl();
  } else if (check(TokenKind::KwImport) || check(TokenKind::KwFrom)) {
    result = parseImportStmt();
  } else if (check(TokenKind::KwAsm)) {
    result = parseAsmStmt();
  } else if (check(TokenKind::At)) {
    result = parseDefDecl();
  } else {
    result = parseStatement();
  }

  if (panicMode_)
    synchronize();
  return result;
}

// ── def ───────────────────────────────────────────────────────

std::unique_ptr<DefDecl> Parser::parseDefDecl() {
  auto loc = current_.location;
  bool isExtern = false;

  if (match(TokenKind::At)) {
    auto attr =
        expect(TokenKind::Identifier, "expected attribute name after '@'")
            .value;
    if (attr == "extern") {
      isExtern = true;
    } else {
      error("unknown attribute");
    }
    // For now, annotations only work on 'def'/'fn'
    skipNewlines();
  }

  bool isFn = false;
  if (match(TokenKind::KwFn)) {
    isFn = true;
  } else if (match(TokenKind::KwDef)) {
    isFn = false;
  } else {
    error("expected 'def' or 'fn'");
  }

  auto name = expect(TokenKind::Identifier, "expected function name").value;

  expect(TokenKind::LParen, "expected '(' after function name");
  auto params = parseParamList();
  expect(TokenKind::RParen, "expected ')' after parameters");

  TypePointer retType = nullptr;
  if (match(TokenKind::Arrow)) {
    retType = parseType();
  }

  bool raises = false;
  TypePointer raisesType = nullptr;
  if (match(TokenKind::KwRaises)) {
    raises = true;
    if (check(TokenKind::Identifier)) {
      raisesType = parseType();
    }
  }

  expect(TokenKind::Colon, "expected ':' before function body");

  std::unique_ptr<Block> body = nullptr;
  if (isExtern) {
    if (match(TokenKind::KwPass)) {
      expectNewline();
    } else {
      expectNewline();
      if (match(TokenKind::Indent)) {
        if (match(TokenKind::KwPass)) {
          expectNewline();
        } else {
          error("extern function must have 'pass' as body if indented");
        }
        expect(TokenKind::Dedent, "expected dedent after extern function");
      }
    }
  } else {
    expectNewline();
    body = parseBlock();
  }

  return std::make_unique<DefDecl>(std::move(name), std::move(params),
                                   std::move(retType), std::move(body), loc,
                                   isExtern, isFn, raises,
                                   std::move(raisesType));
}

// ── Parameter list ────────────────────────────────────────────

std::vector<Parameter> Parser::parseParamList() {
  std::vector<Parameter> params;
  bool keywordOnly = false;

  if (check(TokenKind::RParen))
    return params;

  do {
    auto loc = current_.location;

    // Mojo / for positional only
    if (match(TokenKind::Slash)) {
      for (auto &p : params) {
        p.isPosOnly = true;
      }
      continue;
    }

    // Mojo * for keyword only or *args
    if (match(TokenKind::Star)) {
      if (checkOneOf(TokenKind::Comma, TokenKind::RParen)) {
        keywordOnly = true;
        continue;
      }
      // *args
      auto name = expect(TokenKind::Identifier, "expected parameter name after '*'").value;
      Parameter param;
      param.name = std::move(name);
      param.location = loc;
      param.isVariadic = true;
      if (match(TokenKind::Colon))
        param.type = parseType();
      params.push_back(std::move(param));
      keywordOnly = true;
      continue;
    }

    // Mojo **kwargs
    if (match(TokenKind::Power)) {
      auto name = expect(TokenKind::Identifier, "expected parameter name after '**'").value;
      Parameter param;
      param.name = std::move(name);
      param.location = loc;
      param.isKeywordVariadic = true;
      if (match(TokenKind::Colon))
        param.type = parseType();
      params.push_back(std::move(param));
      break; // **kwargs must be last
    }

    ArgumentConvention conv = ArgumentConvention::None;
    if (match(TokenKind::KwInout))
      conv = ArgumentConvention::Inout;
    else if (match(TokenKind::KwMut))
      conv = ArgumentConvention::Mut;
    else if (match(TokenKind::KwBorrowed))
      conv = ArgumentConvention::Borrowed;
    else if (match(TokenKind::KwOwned))
      conv = ArgumentConvention::Owned;

    Parameter param;
    param.location = loc;
    param.name = expect(TokenKind::Identifier, "expected parameter name").value;
    param.isKeywordOnly = keywordOnly;
    param.convention = conv;

    if (match(TokenKind::Colon)) {
      param.type = parseType();
    }

    params.push_back(std::move(param));
  } while (match(TokenKind::Comma));

  return params;
}

// ── Type parsing ──────────────────────────────────────────────

TypePointer Parser::parseType() {
  auto loc = current_.location;
  auto name = expect(TokenKind::Identifier, "expected type name").value;

  // Check for generic type: Name[T1, T2]
  if (match(TokenKind::LBracket)) {
    std::vector<TypePointer> args;
    do {
      args.push_back(parseType());
    } while (match(TokenKind::Comma));
    expect(TokenKind::RBracket, "expected ']' after type arguments");
    return std::make_unique<GenericType>(std::move(name), std::move(args), loc);
  }

  return std::make_unique<NamedType>(std::move(name), loc);
}

std::unique_ptr<ClassDecl> Parser::parseClassDecl() {
  auto loc = current_.location;
  expect(TokenKind::KwClass, "expected 'class'");

  auto name = expect(TokenKind::Identifier, "expected class name").value;
  expect(TokenKind::Colon, "expected ':' after class name");
  expectNewline();

  std::vector<StmtPtr> methods;

  if (!match(TokenKind::Indent)) {
    error("expected indented class body");
    return std::make_unique<ClassDecl>(std::move(name), std::move(methods),
                                       loc);
  }

  while (!check(TokenKind::Dedent) && !check(TokenKind::Eof)) {
    skipNewlines();

    if (check(TokenKind::Dedent) || check(TokenKind::Eof))
      break;

    if (check(TokenKind::KwDef)) {
      methods.push_back(parseDefDecl());
    } else if (check(TokenKind::KwPass)) {
      advance();
      expectNewline();
    } else {
      error("expected method definition in class");
      advance();
    }
  }

  match(TokenKind::Dedent);

  return std::make_unique<ClassDecl>(std::move(name), std::move(methods), loc);
}

// ── Import ────────────────────────────────────────────────────

std::unique_ptr<ImportStmt> Parser::parseImportStmt() {
  auto loc = current_.location;

  if (match(TokenKind::KwFrom)) {
    auto module = expect(TokenKind::Identifier, "expected module name").value;
    while (match(TokenKind::Dot)) {
      module +=
          "." +
          expect(TokenKind::Identifier, "expected identifier after '.'").value;
    }
    expect(TokenKind::KwImport, "expected 'import' after module name");

    std::vector<std::string> names;
    do {
      names.push_back(
          expect(TokenKind::Identifier, "expected import name").value);
    } while (match(TokenKind::Comma));

    std::string alias;
    if (match(TokenKind::KwAs)) {
      alias = expect(TokenKind::Identifier, "expected alias").value;
    }

    expectNewline();
    return std::make_unique<ImportStmt>(std::move(module), std::move(names),
                                        std::move(alias), loc);
  }

  // import module
  expect(TokenKind::KwImport, "expected 'import'");
  auto module = expect(TokenKind::Identifier, "expected module name").value;
  while (match(TokenKind::Dot)) {
    module +=
        "." +
        expect(TokenKind::Identifier, "expected identifier after '.'").value;
  }

  std::string alias;
  if (match(TokenKind::KwAs)) {
    alias = expect(TokenKind::Identifier, "expected alias").value;
  }

  expectNewline();
  return std::make_unique<ImportStmt>(
      std::move(module), std::vector<std::string>{}, std::move(alias), loc);
}

// ── Inline ASM ────────────────────────────────────────────────
// asm("template", "constraints", args...)

std::unique_ptr<AsmStmt> Parser::parseAsmStmt() {
  auto loc = current_.location;
  expect(TokenKind::KwAsm, "expected 'asm'");
  expect(TokenKind::LParen, "expected '(' after 'asm'");

  std::string templateStr;
  if (match(TokenKind::StringLiteral)) {
    templateStr = previous_.value;
  } else {
    error("expected string literal for assembly template");
  }

  expect(TokenKind::Comma, "expected ',' after template");

  std::string constraintsStr;
  if (match(TokenKind::StringLiteral)) {
    constraintsStr = previous_.value;
  } else {
    error("expected string literal for assembly constraints");
  }

  std::vector<ExpressionPointer> args;
  while (match(TokenKind::Comma)) {
    args.push_back(parseExpression());
  }

  expect(TokenKind::RParen, "expected ')' after asm arguments");
  expectNewline();

  // For now, assume volatile
  return std::make_unique<AsmStmt>(std::move(templateStr),
                                   std::move(constraintsStr), std::move(args),
                                   /*isVolatile=*/true, loc);
}

// ── Statements ────────────────────────────────────────────────

StmtPtr Parser::parseStatement() {
  if (check(TokenKind::KwReturn))
    return parseReturnStmt();
  if (check(TokenKind::KwIf))
    return parseIfStmt();
  if (check(TokenKind::KwWhile))
    return parseWhileStmt();
  if (check(TokenKind::KwFor))
    return parseForStmt();
  if (check(TokenKind::KwVar) || check(TokenKind::KwLet))
    return parseMojoVarDecl();
  if (check(TokenKind::KwTry))
    return parseTryExceptStmt();
  if (check(TokenKind::KwRaise))
    return parseRaiseStmt();


  if (match(TokenKind::KwBreak)) {
    auto loc = previous_.location;
    expectNewline();
    return std::make_unique<BreakStatement>(loc);
  }

  if (match(TokenKind::KwContinue)) {
    auto loc = previous_.location;
    expectNewline();
    return std::make_unique<ContinueStatement>(loc);
  }

  if (match(TokenKind::KwPass)) {
    auto loc = previous_.location;
    expectNewline();
    return std::make_unique<PassStatement>(loc);
  }

  return parseExprStmt();
}

std::unique_ptr<ReturnStatement> Parser::parseReturnStmt() {
  auto loc = current_.location;
  expect(TokenKind::KwReturn, "expected 'return'");

  ExpressionPointer value = nullptr;
  if (!check(TokenKind::Newline) && !check(TokenKind::Eof) &&
      !check(TokenKind::Dedent)) {
    value = parseExpression();
  }

  expectNewline();
  return std::make_unique<ReturnStatement>(std::move(value), loc);
}

std::unique_ptr<IfStatement> Parser::parseIfStmt() {
  auto loc = current_.location;
  advance(); // consume 'if' or 'elif'

  auto condition = parseExpression();
  expect(TokenKind::Colon, "expected ':' after if condition");
  expectNewline();

  auto thenBlock = parseBlock();

  StmtPtr elseBlock = nullptr;
  skipNewlines();

  if (check(TokenKind::KwElif)) {
    elseBlock = parseIfStmt(); // recurse for elif chains
  } else if (match(TokenKind::KwElse)) {
    expect(TokenKind::Colon, "expected ':' after 'else'");
    expectNewline();
    elseBlock = parseBlock();
  }

  return std::make_unique<IfStatement>(
      std::move(condition), std::move(thenBlock), std::move(elseBlock), loc);
}

std::unique_ptr<WhileStatement> Parser::parseWhileStmt() {
  auto loc = current_.location;
  expect(TokenKind::KwWhile, "expected 'while'");

  auto condition = parseExpression();
  expect(TokenKind::Colon, "expected ':' after while condition");
  expectNewline();

  auto body = parseBlock();

  return std::make_unique<WhileStatement>(std::move(condition), std::move(body),
                                          loc);
}

std::unique_ptr<ForStatement> Parser::parseForStmt() {
  auto loc = current_.location;
  expect(TokenKind::KwFor, "expected 'for'");

  auto varName = expect(TokenKind::Identifier, "expected loop variable").value;
  expect(TokenKind::KwIn, "expected 'in' after loop variable");
  auto iterable = parseExpression();
  expect(TokenKind::Colon, "expected ':' after for iterable");
  expectNewline();

  auto body = parseBlock();

  return std::make_unique<ForStatement>(std::move(varName), std::move(iterable),
                                        std::move(body), loc);
}

std::unique_ptr<TryExceptStmt> Parser::parseTryExceptStmt() {
  auto loc = current_.location;
  expect(TokenKind::KwTry, "expected 'try'");
  expect(TokenKind::Colon, "expected ':' after 'try'");
  expectNewline();

  auto tryBlock = parseBlock();
  skipNewlines();

  expect(TokenKind::KwExcept, "expected 'except' after 'try' block");

  std::string var;
  if (match(TokenKind::Identifier)) {
    var = previous_.value;
  }

  expect(TokenKind::Colon, "expected ':' after 'except'");
  expectNewline();

  auto exceptBlock = parseBlock();

  return std::make_unique<TryExceptStmt>(std::move(tryBlock), std::move(var),
                                         std::move(exceptBlock), loc);
}

std::unique_ptr<RaiseStmt> Parser::parseRaiseStmt() {
  auto loc = current_.location;
  expect(TokenKind::KwRaise, "expected 'raise'");

  ExpressionPointer expr = nullptr;
  if (!check(TokenKind::Newline) && !check(TokenKind::Dedent) &&
      !check(TokenKind::Eof)) {
    expr = parseExpression();
  }

  expectNewline();
  return std::make_unique<RaiseStmt>(std::move(expr), loc);
}

StmtPtr Parser::parseExprStmt() {
  auto loc = current_.location;
  auto expr = parseExpression();

  // Treat `name = expr` as an implicit variable declaration
  if (expr->kind == ExprKind::Assign) {
    auto *assign = static_cast<AssignExpr *>(expr.get());
    if (assign->target->kind == ExprKind::Identifier) {
      auto *ident = static_cast<IdentifierExpr *>(assign->target.get());
      std::string name = ident->name;
      ExpressionPointer init = std::move(assign->value);
      expectNewline();
      return std::make_unique<VariableDeclaration>(std::move(name), nullptr,
                                                   std::move(init), loc);
    }
  }

  expectNewline();
  return std::make_unique<ExpressionStatement>(std::move(expr), loc);
}

std::unique_ptr<VariableDeclaration> Parser::parseMojoVarDecl() {
  auto loc = current_.location;
  bool isVar = match(TokenKind::KwVar);
  bool isLet = false;
  if (!isVar) {
    expect(TokenKind::KwLet, "expected 'var' or 'let'");
    isLet = true;
  }

  auto name = expect(TokenKind::Identifier, "expected variable name").value;
  TypePointer type = nullptr;
  if (match(TokenKind::Colon)) {
    type = parseType();
  }

  ExpressionPointer init = nullptr;
  if (match(TokenKind::Assign)) {
    init = parseExpression();
  }

  expectNewline();
  return std::make_unique<VariableDeclaration>(
      std::move(name), std::move(type), std::move(init), loc, isVar, isLet);
}

std::unique_ptr<StructDecl> Parser::parseStructDecl() {
  auto loc = current_.location;
  expect(TokenKind::KwStruct, "expected 'struct'");
  auto name = expect(TokenKind::Identifier, "expected struct name").value;
  expect(TokenKind::Colon, "expected ':' after struct name");
  expectNewline();

  std::vector<StmtPtr> members;

  if (!match(TokenKind::Indent)) {
    error("expected indented struct body");
    return std::make_unique<StructDecl>(std::move(name), std::move(members),
                                        loc);
  }

  while (!check(TokenKind::Dedent) && !check(TokenKind::Eof)) {
    skipNewlines();
    if (check(TokenKind::Dedent) || check(TokenKind::Eof))
      break;

    if (check(TokenKind::KwVar) || check(TokenKind::KwLet)) {
      members.push_back(parseMojoVarDecl());
    } else if (check(TokenKind::KwDef) || check(TokenKind::KwFn)) {
      members.push_back(parseDefDecl());
    } else {
      error("expected field or method in struct");
      advance();
    }
  }

  match(TokenKind::Dedent);
  return std::make_unique<StructDecl>(std::move(name), std::move(members), loc);
}

std::unique_ptr<AliasDecl> Parser::parseAliasDecl() {
  auto loc = current_.location;
  expect(TokenKind::KwAlias, "expected 'alias'");
  auto name = expect(TokenKind::Identifier, "expected alias name").value;
  expect(TokenKind::Assign, "expected '=' in alias declaration");
  auto value = parseExpression();
  expectNewline();
  return std::make_unique<AliasDecl>(std::move(name), std::move(value), loc);
}

// ── Block ─────────────────────────────────────────────────────

std::unique_ptr<Block> Parser::parseBlock() {
  auto loc = current_.location;
  std::vector<StmtPtr> stmts;

  if (!match(TokenKind::Indent)) {
    error("expected indented block");
    return std::make_unique<Block>(std::move(stmts), loc);
  }

  while (!check(TokenKind::Dedent) && !check(TokenKind::Eof)) {
    skipNewlines();
    if (check(TokenKind::Dedent) || check(TokenKind::Eof))
      break;

    auto stmt = parseDeclaration();
    if (stmt)
      stmts.push_back(std::move(stmt));
  }

  match(TokenKind::Dedent);

  return std::make_unique<Block>(std::move(stmts), loc);
}

// ── Expressions ───────────────────────────────────────────────

ExpressionPointer Parser::parseExpression() { return parseAssignment(); }

ExpressionPointer Parser::parseAssignment() {
  auto expr = parseOr();

  if (match(TokenKind::Assign)) {
    auto loc = previous_.location;
    auto value = parseAssignment(); // right-associative
    return std::make_unique<AssignExpr>(std::move(expr), std::move(value), loc);
  }

  // Compound assignment
  if (check(TokenKind::PlusAssign) || check(TokenKind::MinusAssign) ||
      check(TokenKind::StarAssign) || check(TokenKind::SlashAssign)) {
    auto loc = current_.location;
    CompoundAssignOp op;
    switch (current_.kind) {
    case TokenKind::PlusAssign:
      op = CompoundAssignOp::AddAssign;
      break;
    case TokenKind::MinusAssign:
      op = CompoundAssignOp::SubAssign;
      break;
    case TokenKind::StarAssign:
      op = CompoundAssignOp::MulAssign;
      break;
    case TokenKind::SlashAssign:
      op = CompoundAssignOp::DivAssign;
      break;
    default:
      __builtin_unreachable();
    }
    advance();
    auto value = parseExpression();
    return std::make_unique<CompoundAssignExpr>(op, std::move(expr),
                                                std::move(value), loc);
  }

  return expr;
}

ExpressionPointer Parser::parseOr() {
  auto left = parseAnd();
  while (match(TokenKind::KwOr)) {
    auto loc = previous_.location;
    auto right = parseAnd();
    left = std::make_unique<BinaryOpExpr>(BinaryOp::Or, std::move(left),
                                          std::move(right), loc);
  }
  return left;
}

ExpressionPointer Parser::parseAnd() {
  auto left = parseEquality();
  while (match(TokenKind::KwAnd)) {
    auto loc = previous_.location;
    auto right = parseEquality();
    left = std::make_unique<BinaryOpExpr>(BinaryOp::And, std::move(left),
                                          std::move(right), loc);
  }
  return left;
}

ExpressionPointer Parser::parseEquality() {
  auto left = parseComparison();
  while (check(TokenKind::EqualEqual) || check(TokenKind::NotEqual)) {
    auto loc = current_.location;
    bool isEq = current_.kind == TokenKind::EqualEqual;
    advance();
    auto right = parseComparison();
    left =
        std::make_unique<BinaryOpExpr>(isEq ? BinaryOp::Eq : BinaryOp::Neq,
                                       std::move(left), std::move(right), loc);
  }
  return left;
}

ExpressionPointer Parser::parseComparison() {
  auto left = parseBitwiseOr();
  while (check(TokenKind::Less) || check(TokenKind::LessEqual) ||
         check(TokenKind::Greater) || check(TokenKind::GreaterEqual)) {
    auto loc = current_.location;
    BinaryOp op;
    switch (current_.kind) {
    case TokenKind::Less:
      op = BinaryOp::Lt;
      break;
    case TokenKind::LessEqual:
      op = BinaryOp::Lte;
      break;
    case TokenKind::Greater:
      op = BinaryOp::Gt;
      break;
    case TokenKind::GreaterEqual:
      op = BinaryOp::Gte;
      break;
    default:
      __builtin_unreachable();
    }
    advance();
    auto right = parseBitwiseOr();
    left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right),
                                          loc);
  }
  return left;
}

ExpressionPointer Parser::parseBitwiseOr() {
  auto left = parseBitwiseXor();
  while (match(TokenKind::Pipe)) {
    auto loc = previous_.location;
    auto right = parseBitwiseXor();
    left = std::make_unique<BinaryOpExpr>(BinaryOp::BitwiseOr, std::move(left),
                                          std::move(right), loc);
  }
  return left;
}

ExpressionPointer Parser::parseBitwiseXor() {
  auto left = parseBitwiseAnd();
  while (match(TokenKind::Caret)) {
    auto loc = previous_.location;
    auto right = parseBitwiseAnd();
    left = std::make_unique<BinaryOpExpr>(BinaryOp::BitwiseXor, std::move(left),
                                          std::move(right), loc);
  }
  return left;
}

ExpressionPointer Parser::parseBitwiseAnd() {
  auto left = parseTerm();
  while (match(TokenKind::Ampersand)) {
    auto loc = previous_.location;
    auto right = parseTerm();
    left = std::make_unique<BinaryOpExpr>(BinaryOp::BitwiseAnd, std::move(left),
                                          std::move(right), loc);
  }
  return left;
}

ExpressionPointer Parser::parseTerm() {
  auto left = parseFactor();
  while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
    auto loc = current_.location;
    bool isAdd = current_.kind == TokenKind::Plus;
    advance();
    auto right = parseFactor();
    left =
        std::make_unique<BinaryOpExpr>(isAdd ? BinaryOp::Add : BinaryOp::Sub,
                                       std::move(left), std::move(right), loc);
  }
  return left;
}

ExpressionPointer Parser::parseFactor() {
  auto left = parsePower();
  while (check(TokenKind::Star) || check(TokenKind::Slash) ||
         check(TokenKind::Percent)) {
    auto loc = current_.location;
    BinaryOp op;
    switch (current_.kind) {
    case TokenKind::Star:
      op = BinaryOp::Mul;
      break;
    case TokenKind::Slash:
      op = BinaryOp::Div;
      break;
    case TokenKind::Percent:
      op = BinaryOp::Mod;
      break;
    default:
      __builtin_unreachable();
    }
    advance();
    auto right = parsePower();
    left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right),
                                          loc);
  }
  return left;
}

ExpressionPointer Parser::parsePower() {
  auto base = parseUnary();
  if (match(TokenKind::Power)) {
    auto loc = previous_.location;
    auto exp = parsePower(); // right-associative
    return std::make_unique<BinaryOpExpr>(BinaryOp::Power, std::move(base),
                                          std::move(exp), loc);
  }
  return base;
}

ExpressionPointer Parser::parseUnary() {
  if (match(TokenKind::Minus)) {
    auto loc = previous_.location;
    auto operand = parseUnary();
    return std::make_unique<UnaryOpExpr>(UnaryOp::Negate, std::move(operand),
                                         loc);
  }
  if (match(TokenKind::KwNot)) {
    auto loc = previous_.location;
    auto operand = parseUnary();
    return std::make_unique<UnaryOpExpr>(UnaryOp::Not, std::move(operand), loc);
  }
  if (match(TokenKind::Tilde)) {
    auto loc = previous_.location;
    auto operand = parseUnary();
    return std::make_unique<UnaryOpExpr>(UnaryOp::BitwiseNot,
                                         std::move(operand), loc);
  }
  return parsePostfix();
}

ExpressionPointer Parser::parsePostfix() {
  auto expr = parsePrimary();

  while (true) {
    if (check(TokenKind::LParen)) {
      expr = parseCallArgs(std::move(expr));
    } else if (match(TokenKind::Dot)) {
      expr = parseMemberAccess(std::move(expr));
    } else if (check(TokenKind::LBracket)) {
      expr = parseIndexAccess(std::move(expr));
    } else {
      break;
    }
  }

  return expr;
}

ExpressionPointer Parser::parsePrimary() {
  auto loc = current_.location;

  // Integer literal
  if (match(TokenKind::IntLiteral)) {
    int64_t val = 0;
    auto &s = previous_.value;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
      std::string clean;
      for (size_t i = 2; i < s.size(); ++i)
        if (s[i] != '_')
          clean += s[i];
      std::from_chars(clean.data(), clean.data() + clean.size(), val, 16);
    } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
      std::string clean;
      for (size_t i = 2; i < s.size(); ++i)
        if (s[i] != '_')
          clean += s[i];
      std::from_chars(clean.data(), clean.data() + clean.size(), val, 2);
    } else {
      std::string clean;
      for (char c : s)
        if (c != '_')
          clean += c;
      std::from_chars(clean.data(), clean.data() + clean.size(), val);
    }
    return std::make_unique<IntLiteralExpr>(val, loc);
  }

  // Float literal
  if (match(TokenKind::FloatLiteral)) {
    double val = std::stod(previous_.value);
    return std::make_unique<FloatLiteralExpr>(val, loc);
  }

  // String literal
  if (match(TokenKind::StringLiteral)) {
    return std::make_unique<StringLiteralExpr>(previous_.value, loc);
  }

  // True / False
  if (match(TokenKind::KwTrue)) {
    return std::make_unique<BoolLiteralExpr>(true, loc);
  }
  if (match(TokenKind::KwFalse)) {
    return std::make_unique<BoolLiteralExpr>(false, loc);
  }

  // None
  if (match(TokenKind::KwNone)) {
    return std::make_unique<NoneLiteralExpr>(loc);
  }

  // Identifier
  if (match(TokenKind::Identifier)) {
    return std::make_unique<IdentifierExpr>(previous_.value, loc);
  }

  // Grouped expression
  if (match(TokenKind::LParen)) {
    auto expr = parseExpression();
    expect(TokenKind::RParen, "expected ')' after expression");
    return expr;
  }

  error("expected expression");
  return std::make_unique<IntLiteralExpr>(0, loc); // error recovery
}

// ── Postfix helpers ───────────────────────────────────────────

ExpressionPointer Parser::parseCallArgs(ExpressionPointer callee) {
  auto loc = current_.location;
  expect(TokenKind::LParen, "expected '('");

  std::vector<ExpressionPointer> args;
  if (!check(TokenKind::RParen)) {
    do {
      args.push_back(parseExpression());
    } while (match(TokenKind::Comma));
  }

  expect(TokenKind::RParen, "expected ')' after arguments");
  return std::make_unique<CallExpr>(std::move(callee), std::move(args), loc);
}

ExpressionPointer Parser::parseMemberAccess(ExpressionPointer object) {
  auto loc = previous_.location;
  auto member = expect(TokenKind::Identifier, "expected member name").value;
  return std::make_unique<MemberAccessExpr>(std::move(object),
                                            std::move(member), loc);
}

ExpressionPointer Parser::parseIndexAccess(ExpressionPointer object) {
  auto loc = current_.location;
  expect(TokenKind::LBracket, "expected '['");
  auto index = parseExpression();
  expect(TokenKind::RBracket, "expected ']'");
  return std::make_unique<IndexExpr>(std::move(object), std::move(index), loc);
}

} // namespace capybara
