#include "capybara/lexer.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cctype>

namespace capybara {

Lexer::Lexer(llvm::StringRef source, llvm::StringRef filename)
    : source_(source), filename_(filename), current_(source.data()),
      end_(source.data() + source.size()) {}

char Lexer::peekChar() const {
  if (isAtEnd())
    return '\0';
  return *current_;
}

char Lexer::peekChar(int offset) const {
  if (current_ + offset >= end_)
    return '\0';
  return *(current_ + offset);
}

char Lexer::advance() {
  if (isAtEnd())
    return '\0';
  char c = *current_++;
  if (c == '\n') {
    line_++;
    column_ = 1;
  } else {
    column_++;
  }
  return c;
}

bool Lexer::isAtEnd() const { return current_ >= end_; }

bool Lexer::match(char expected) {
  if (isAtEnd() || *current_ != expected)
    return false;
  advance();
  return true;
}

void Lexer::skipLineComment() {
  while (!isAtEnd() && peekChar() != '\n') {
    advance();
  }
}

void Lexer::skipWhitespaceInLine() {
  while (!isAtEnd() && (peekChar() == ' ' || peekChar() == '\t')) {
    advance();
  }
}

SourceLocation Lexer::currentLocation() const {
  SourceLocation loc;
  loc.line = line_;
  loc.column = column_;
  loc.smloc = llvm::SMLoc::getFromPointer(current_);
  return loc;
}

Token Lexer::makeToken(TokenKind kind, const std::string &value,
                       SourceLocation loc) {
  Token tok;
  tok.kind = kind;
  tok.value = value;
  tok.location = loc;
  return tok;
}

Token Lexer::makeError(const std::string &message, SourceLocation loc) {
  errorCount_++;
  llvm::errs() << filename_ << ":" << loc.line << ":" << loc.column
               << ": error: " << message << "\n";
  return makeToken(TokenKind::Error, message, loc);
}

// ── Keyword lookup ────────────────────────────────────────────

TokenKind Lexer::lookupKeyword(llvm::StringRef word) {
  // clang-format off
    if (word == "def")      return TokenKind::KwDef;
    if (word == "return")   return TokenKind::KwReturn;
    if (word == "if")       return TokenKind::KwIf;
    if (word == "else")     return TokenKind::KwElse;
    if (word == "elif")     return TokenKind::KwElif;
    if (word == "while")    return TokenKind::KwWhile;
    if (word == "for")      return TokenKind::KwFor;
    if (word == "in")       return TokenKind::KwIn;
    if (word == "class")    return TokenKind::KwClass;
    if (word == "True")     return TokenKind::KwTrue;
    if (word == "False")    return TokenKind::KwFalse;
    if (word == "None")     return TokenKind::KwNone;
    if (word == "and")      return TokenKind::KwAnd;
    if (word == "or")       return TokenKind::KwOr;
    if (word == "not")      return TokenKind::KwNot;
    if (word == "import")   return TokenKind::KwImport;
    if (word == "from")     return TokenKind::KwFrom;
    if (word == "as")       return TokenKind::KwAs;
    if (word == "break")    return TokenKind::KwBreak;
    if (word == "continue") return TokenKind::KwContinue;
    if (word == "pass")     return TokenKind::KwPass;
    if (word == "asm")      return TokenKind::KwAsm;
    if (word == "fn")       return TokenKind::KwFn;
    if (word == "var")      return TokenKind::KwVar;
    if (word == "let")      return TokenKind::KwLet;
    if (word == "struct")   return TokenKind::KwStruct;
    if (word == "alias")    return TokenKind::KwAlias;
    if (word == "try")      return TokenKind::KwTry;
    if (word == "except")   return TokenKind::KwExcept;
    if (word == "raises")   return TokenKind::KwRaises;
    if (word == "raise")    return TokenKind::KwRaise;
    if (word == "inout")    return TokenKind::KwInout;
    if (word == "mut")      return TokenKind::KwMut;
    if (word == "borrowed") return TokenKind::KwBorrowed;
    if (word == "owned")    return TokenKind::KwOwned;
  // clang-format on
  return TokenKind::Identifier;
}

// ── Indentation handling ──────────────────────────────────────

Token Lexer::lexIndentation() {
  auto loc = currentLocation();

  // Count leading spaces (tabs converted to 4 spaces)
  unsigned indent = 0;
  while (!isAtEnd()) {
    if (peekChar() == ' ') {
      indent++;
      advance();
    } else if (peekChar() == '\t') {
      indent = (indent + 4) & ~3u; // round up to next tab stop
      advance();
    } else {
      break;
    }
  }

  // Blank line or comment-only line: skip entirely
  if (isAtEnd() || peekChar() == '\n' || peekChar() == '#') {
    atLineStart_ = false; // don't re-enter indentation mode until next \n
    // Skip the blank/comment line
    if (!isAtEnd() && peekChar() == '#') {
      skipLineComment();
    }
    if (!isAtEnd() && peekChar() == '\n') {
      advance();
      atLineStart_ = true;
    }
    // Recurse to get the next real token
    return lexTokenInternal();
  }

  unsigned currentIndent = indentStack_.back();
  atLineStart_ = false;

  if (indent > currentIndent) {
    indentStack_.push_back(indent);
    return makeToken(TokenKind::Indent, "<INDENT>", loc);
  }

  if (indent < currentIndent) {
    // Count how many levels we need to dedent
    pendingDedents_ = 0;
    while (indentStack_.size() > 1 && indentStack_.back() > indent) {
      indentStack_.pop_back();
      pendingDedents_++;
    }
    if (indentStack_.back() != indent) {
      return makeError("inconsistent indentation", loc);
    }
    pendingDedents_--;
    return makeToken(TokenKind::Dedent, "<DEDENT>", loc);
  }

  // Same indentation level — just continue with normal lexing
  return lexTokenInternal();
}

// ── Number lexing ─────────────────────────────────────────────

Token Lexer::lexNumber() {
  auto loc = currentLocation();
  const char *start = current_;
  bool isFloat = false;

  // Hex: 0x...
  if (peekChar() == '0' && (peekChar(1) == 'x' || peekChar(1) == 'X')) {
    advance();
    advance(); // skip 0x
    while (!isAtEnd() && (std::isxdigit(peekChar()) || peekChar() == '_')) {
      advance();
    }
    return makeToken(TokenKind::IntLiteral,
                     std::string(start, current_ - start), loc);
  }

  // Binary: 0b...
  if (peekChar() == '0' && (peekChar(1) == 'b' || peekChar(1) == 'B')) {
    advance();
    advance();
    while (!isAtEnd() &&
           (peekChar() == '0' || peekChar() == '1' || peekChar() == '_')) {
      advance();
    }
    return makeToken(TokenKind::IntLiteral,
                     std::string(start, current_ - start), loc);
  }

  // Decimal digits
  while (!isAtEnd() && (std::isdigit(peekChar()) || peekChar() == '_')) {
    advance();
  }

  // Fractional part
  if (peekChar() == '.' && std::isdigit(peekChar(1))) {
    isFloat = true;
    advance(); // consume '.'
    while (!isAtEnd() && (std::isdigit(peekChar()) || peekChar() == '_')) {
      advance();
    }
  }

  // Exponent
  if (peekChar() == 'e' || peekChar() == 'E') {
    isFloat = true;
    advance();
    if (peekChar() == '+' || peekChar() == '-')
      advance();
    while (!isAtEnd() && std::isdigit(peekChar())) {
      advance();
    }
  }

  std::string text(start, current_ - start);
  return makeToken(isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral,
                   text, loc);
}

// ── String lexing ─────────────────────────────────────────────

Token Lexer::lexString() {
  auto loc = currentLocation();
  char quote = advance(); // consume opening quote
  std::string value;

  // Triple-quoted strings
  bool triple = false;
  if (peekChar() == quote && peekChar(1) == quote) {
    triple = true;
    advance();
    advance();
  }

  while (!isAtEnd()) {
    if (triple) {
      if (peekChar() == quote && peekChar(1) == quote && peekChar(2) == quote) {
        advance();
        advance();
        advance();
        return makeToken(TokenKind::StringLiteral, value, loc);
      }
    } else {
      if (peekChar() == quote) {
        advance();
        return makeToken(TokenKind::StringLiteral, value, loc);
      }
      if (peekChar() == '\n') {
        return makeError("unterminated string literal", loc);
      }
    }

    // Escape sequences
    if (peekChar() == '\\') {
      advance();
      switch (peekChar()) {
      case 'n':
        value += '\n';
        advance();
        break;
      case 't':
        value += '\t';
        advance();
        break;
      case 'r':
        value += '\r';
        advance();
        break;
      case '\\':
        value += '\\';
        advance();
        break;
      case '\'':
        value += '\'';
        advance();
        break;
      case '"':
        value += '"';
        advance();
        break;
      case '0':
        value += '\0';
        advance();
        break;
      default:
        value += '\\';
        value += advance();
        break;
      }
    } else {
      value += advance();
    }
  }

  return makeError("unterminated string literal", loc);
}

// ── Identifier / keyword lexing ──────────────────────────────

Token Lexer::lexIdentifierOrKeyword() {
  auto loc = currentLocation();
  const char *start = current_;

  while (!isAtEnd() && (std::isalnum(peekChar()) || peekChar() == '_')) {
    advance();
  }

  std::string word(start, current_ - start);
  TokenKind kind = lookupKeyword(word);

  return makeToken(kind, word, loc);
}

// ── Operator / delimiter lexing ──────────────────────────────

Token Lexer::lexOperatorOrDelimiter() {
  auto loc = currentLocation();
  char c = advance();

  switch (c) {
  case '(':
    return makeToken(TokenKind::LParen, "(", loc);
  case ')':
    return makeToken(TokenKind::RParen, ")", loc);
  case '[':
    return makeToken(TokenKind::LBracket, "[", loc);
  case ']':
    return makeToken(TokenKind::RBracket, "]", loc);
  case '{':
    return makeToken(TokenKind::LBrace, "{", loc);
  case '}':
    return makeToken(TokenKind::RBrace, "}", loc);
  case ':':
    return makeToken(TokenKind::Colon, ":", loc);
  case ',':
    return makeToken(TokenKind::Comma, ",", loc);
  case '.':
    return makeToken(TokenKind::Dot, ".", loc);
  case ';':
    return makeToken(TokenKind::Semicolon, ";", loc);
  case '@':
    return makeToken(TokenKind::At, "@", loc);
  case '~':
    return makeToken(TokenKind::Tilde, "~", loc);
  case '&':
    return makeToken(TokenKind::Ampersand, "&", loc);
  case '|':
    return makeToken(TokenKind::Pipe, "|", loc);
  case '^':
    return makeToken(TokenKind::Caret, "^", loc);

  case '+':
    if (match('='))
      return makeToken(TokenKind::PlusAssign, "+=", loc);
    return makeToken(TokenKind::Plus, "+", loc);

  case '-':
    if (match('>'))
      return makeToken(TokenKind::Arrow, "->", loc);
    if (match('='))
      return makeToken(TokenKind::MinusAssign, "-=", loc);
    return makeToken(TokenKind::Minus, "-", loc);

  case '*':
    if (match('*'))
      return makeToken(TokenKind::Power, "**", loc);
    if (match('='))
      return makeToken(TokenKind::StarAssign, "*=", loc);
    return makeToken(TokenKind::Star, "*", loc);

  case '/':
    if (match('='))
      return makeToken(TokenKind::SlashAssign, "/=", loc);
    return makeToken(TokenKind::Slash, "/", loc);

  case '%':
    return makeToken(TokenKind::Percent, "%", loc);

  case '=':
    if (match('='))
      return makeToken(TokenKind::EqualEqual, "==", loc);
    return makeToken(TokenKind::Assign, "=", loc);

  case '!':
    if (match('='))
      return makeToken(TokenKind::NotEqual, "!=", loc);
    return makeError("unexpected character '!'", loc);

  case '<':
    if (match('='))
      return makeToken(TokenKind::LessEqual, "<=", loc);
    return makeToken(TokenKind::Less, "<", loc);

  case '>':
    if (match('='))
      return makeToken(TokenKind::GreaterEqual, ">=", loc);
    return makeToken(TokenKind::Greater, ">", loc);

  default:
    return makeError(std::string("unexpected character '") + c + "'", loc);
  }
}

// ── Main lex entry point ─────────────────────────────────────

Token Lexer::lexTokenInternal() {
  // Emit pending dedents
  if (pendingDedents_ > 0) {
    pendingDedents_--;
    return makeToken(TokenKind::Dedent, "<DEDENT>", currentLocation());
  }

  // Emit pending newline
  if (pendingNewline_) {
    pendingNewline_ = false;
    return makeToken(TokenKind::Newline, "\\n", currentLocation());
  }

  // Handle indentation at the start of a line
  if (atLineStart_) {
    return lexIndentation();
  }

  // Skip inline whitespace
  skipWhitespaceInLine();

  if (isAtEnd()) {
    // Emit remaining dedents at EOF
    if (indentStack_.size() > 1) {
      indentStack_.pop_back();
      return makeToken(TokenKind::Dedent, "<DEDENT>", currentLocation());
    }
    return makeToken(TokenKind::Eof, "", currentLocation());
  }

  auto loc = currentLocation();
  char c = peekChar();

  // Newline
  if (c == '\n') {
    advance();
    atLineStart_ = true;
    return makeToken(TokenKind::Newline, "\\n", loc);
  }

  // Comment
  if (c == '#') {
    skipLineComment();
    // After a comment, if we hit a newline, treat it as a newline token
    if (!isAtEnd() && peekChar() == '\n') {
      loc = currentLocation();
      advance();
      atLineStart_ = true;
      return makeToken(TokenKind::Newline, "\\n", loc);
    }
    return lexTokenInternal();
  }

  // Numbers
  if (std::isdigit(c)) {
    return lexNumber();
  }

  // Strings
  if (c == '"' || c == '\'') {
    return lexString();
  }

  // Identifiers / keywords
  if (std::isalpha(c) || c == '_') {
    return lexIdentifierOrKeyword();
  }

  // Operators and delimiters
  return lexOperatorOrDelimiter();
}

Token Lexer::nextToken() {
  if (hasPeeked_) {
    hasPeeked_ = false;
    return peekedToken_;
  }
  return lexTokenInternal();
}

const Token &Lexer::peek() {
  if (!hasPeeked_) {
    peekedToken_ = lexTokenInternal();
    hasPeeked_ = true;
  }
  return peekedToken_;
}

std::vector<Token> Lexer::lexAll() {
  std::vector<Token> tokens;
  while (true) {
    Token tok = nextToken();
    tokens.push_back(tok);
    if (tok.kind == TokenKind::Eof)
      break;
  }
  return tokens;
}

// ── Token kind name ──────────────────────────────────────────

const char *Token::kindName(TokenKind kind) {
  switch (kind) {
  case TokenKind::Eof:
    return "EOF";
  case TokenKind::Error:
    return "ERROR";
  case TokenKind::Newline:
    return "NEWLINE";
  case TokenKind::Indent:
    return "INDENT";
  case TokenKind::Dedent:
    return "DEDENT";
  case TokenKind::IntLiteral:
    return "INT";
  case TokenKind::FloatLiteral:
    return "FLOAT";
  case TokenKind::StringLiteral:
    return "STRING";
  case TokenKind::BoolLiteral:
    return "BOOL";
  case TokenKind::Identifier:
    return "IDENT";
  case TokenKind::KwDef:
    return "def";
  case TokenKind::KwReturn:
    return "return";
  case TokenKind::KwIf:
    return "if";
  case TokenKind::KwElse:
    return "else";
  case TokenKind::KwElif:
    return "elif";
  case TokenKind::KwWhile:
    return "while";
  case TokenKind::KwFor:
    return "for";
  case TokenKind::KwIn:
    return "in";
  case TokenKind::KwClass:
    return "class";
  case TokenKind::KwTrue:
    return "True";
  case TokenKind::KwFalse:
    return "False";
  case TokenKind::KwNone:
    return "None";
  case TokenKind::KwAnd:
    return "and";
  case TokenKind::KwOr:
    return "or";
  case TokenKind::KwNot:
    return "not";
  case TokenKind::KwImport:
    return "import";
  case TokenKind::KwFrom:
    return "from";
  case TokenKind::KwAs:
    return "as";
  case TokenKind::KwBreak:
    return "break";
  case TokenKind::KwContinue:
    return "continue";
  case TokenKind::KwPass:
    return "pass";
  case TokenKind::KwAsm:
    return "asm";
  case TokenKind::KwFn:
    return "fn";
  case TokenKind::KwVar:
    return "var";
  case TokenKind::KwLet:
    return "let";
  case TokenKind::KwStruct:
    return "struct";
  case TokenKind::KwAlias:
    return "alias";
  case TokenKind::KwTry:
    return "try";
  case TokenKind::KwExcept:
    return "except";
  case TokenKind::KwRaises:
    return "raises";
  case TokenKind::KwRaise:
    return "raise";
  case TokenKind::KwInout:
    return "inout";
  case TokenKind::KwMut:
    return "mut";
  case TokenKind::KwBorrowed:
    return "borrowed";
  case TokenKind::KwOwned:
    return "owned";
  case TokenKind::Plus:
    return "+";
  case TokenKind::Minus:
    return "-";
  case TokenKind::Star:
    return "*";
  case TokenKind::Slash:
    return "/";
  case TokenKind::Percent:
    return "%";
  case TokenKind::Power:
    return "**";
  case TokenKind::Assign:
    return "=";
  case TokenKind::EqualEqual:
    return "==";
  case TokenKind::NotEqual:
    return "!=";
  case TokenKind::Less:
    return "<";
  case TokenKind::LessEqual:
    return "<=";
  case TokenKind::Greater:
    return ">";
  case TokenKind::GreaterEqual:
    return ">=";
  case TokenKind::Arrow:
    return "->";
  case TokenKind::PlusAssign:
    return "+=";
  case TokenKind::MinusAssign:
    return "-=";
  case TokenKind::StarAssign:
    return "*=";
  case TokenKind::SlashAssign:
    return "/=";
  case TokenKind::LParen:
    return "(";
  case TokenKind::RParen:
    return ")";
  case TokenKind::LBracket:
    return "[";
  case TokenKind::RBracket:
    return "]";
  case TokenKind::LBrace:
    return "{";
  case TokenKind::RBrace:
    return "}";
  case TokenKind::Colon:
    return ":";
  case TokenKind::Comma:
    return ",";
  case TokenKind::Dot:
    return ".";
  case TokenKind::Semicolon:
    return ";";
  case TokenKind::At:
    return "@";
  case TokenKind::Ampersand:
    return "&";
  case TokenKind::Pipe:
    return "|";
  case TokenKind::Caret:
    return "^";
  case TokenKind::Tilde:
    return "~";
  }
  return "UNKNOWN";
}

} // namespace capybara
