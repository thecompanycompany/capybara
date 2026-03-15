#pragma once

#include "llvm/Support/SMLoc.h"
#include <cstdint>
#include <string>

namespace capybara {

/// Source location tracking
struct SourceLocation {
  uint32_t line = 1;
  uint32_t column = 1;
  llvm::SMLoc smloc;
};

/// All token kinds in the Capybara language
enum class TokenKind : uint8_t {
  // Special
  Eof,
  Error,
  Newline,
  Indent, // indentation increased
  Dedent, // indentation decreased

  // Literals
  IntLiteral,
  FloatLiteral,
  StringLiteral,
  BoolLiteral, // true, false

  // Identifiers
  Identifier,

  // Keywords
  KwDef,      // def
  KwReturn,   // return
  KwIf,       // if
  KwElse,     // else
  KwElif,     // elif
  KwWhile,    // while
  KwFor,      // for
  KwIn,       // in
  KwClass,    // class
  KwTrue,     // True
  KwFalse,    // False
  KwNone,     // None
  KwAnd,      // and
  KwOr,       // or
  KwNot,      // not
  KwImport,   // import
  KwFrom,     // from
  KwAs,       // as
  KwBreak,    // break
  KwContinue, // continue
  KwPass,     // pass
  KwAsm,      // asm
  KwFn,       // fn
  KwVar,      // var
  KwLet,      // let
  KwStruct,   // struct
  KwAlias,    // alias
  KwTry,      // try
  KwExcept,   // except
  KwRaises,   // raises
  KwRaise,    // raise
  KwInout,    // inout
  KwMut,      // mut
  KwBorrowed, // borrowed
  KwOwned,    // owned

  // Operators
  Plus,         // +
  Minus,        // -
  Star,         // *
  Slash,        // /
  Percent,      // %
  Power,        // **
  Assign,       // =
  EqualEqual,   // ==
  NotEqual,     // !=
  Less,         // <
  LessEqual,    // <=
  Greater,      // >
  GreaterEqual, // >=
  Arrow,        // ->
  PlusAssign,   // +=
  MinusAssign,  // -=
  StarAssign,   // *=
  SlashAssign,  // /=

  // Delimiters
  LParen,    // (
  RParen,    // )
  LBracket,  // [
  RBracket,  // ]
  LBrace,    // {
  RBrace,    // }
  Colon,     // :
  Comma,     // ,
  Dot,       // .
  Semicolon, // ;
  At,        // @
  Ampersand, // &
  Pipe,      // |
  Caret,     // ^
  Tilde,     // ~
};

/// A single token produced by the lexer
struct Token {
  TokenKind kind = TokenKind::Eof;
  std::string value; // raw text of the token
  SourceLocation location;

  /// Human-readable name for a token kind
  static const char *kindName(TokenKind kind);

  bool is(TokenKind k) const { return kind == k; }
  bool isNot(TokenKind k) const { return kind != k; }

  template <typename... Ts> bool isOneOf(Ts... kinds) const {
    return (is(kinds) || ...);
  }

  /// Is this token a keyword?
  bool isKeyword() const {
    return kind >= TokenKind::KwDef && kind <= TokenKind::KwOwned;
  }

  /// Is this an operator?
  bool isOperator() const {
    return kind >= TokenKind::Plus && kind <= TokenKind::SlashAssign;
  }
};

} // namespace capybara
