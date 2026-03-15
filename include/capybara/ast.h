#pragma once

#include "capybara/token.h"
#include <memory>
#include <string>
#include <vector>

namespace capybara {

// Forward declarations
struct Expr;
struct Statement;
struct TypeExpr;

using ExpressionPointer = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Statement>;
using TypePointer = std::unique_ptr<TypeExpr>;

// ============================================================
// Type expressions
// ============================================================

struct NamedType;
struct GenericType;

struct TypeExprVisitor {
  virtual ~TypeExprVisitor() = default;
  virtual void visit(const NamedType &node) = 0;
  virtual void visit(const GenericType &node) = 0;
};

enum class TypeExprKind : uint8_t {
  Named,   // Int, Float, String, MyStruct
  Generic, // List[Int], Optional[String]
  Pointer, // Pointer[Int] or &Int
};

struct TypeExpr {
  TypeExprKind kind;
  SourceLocation location;

  TypeExpr(TypeExprKind k, SourceLocation loc) : kind(k), location(loc) {}
  virtual ~TypeExpr() = default;

  virtual void accept(TypeExprVisitor &visitor) const = 0;

  /// Pretty-print the type
  virtual std::string toString() const;
};

struct NamedType : TypeExpr {
  std::string name;

  NamedType(std::string n, SourceLocation loc)
      : TypeExpr(TypeExprKind::Named, loc), name(std::move(n)) {}

  void accept(TypeExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct GenericType : TypeExpr {
  std::string name;
  std::vector<TypePointer> typeArgs;

  GenericType(std::string n, std::vector<TypePointer> args, SourceLocation loc)
      : TypeExpr(TypeExprKind::Generic, loc), name(std::move(n)),
        typeArgs(std::move(args)) {}

  void accept(TypeExprVisitor &visitor) const override { visitor.visit(*this); }
};

// ============================================================
// Expressions
// ============================================================

struct IntLiteralExpr;
struct FloatLiteralExpr;
struct StringLiteralExpr;
struct BoolLiteralExpr;
struct NoneLiteralExpr;
struct IdentifierExpr;
struct BinaryOpExpr;
struct UnaryOpExpr;
struct CallExpr;
struct MemberAccessExpr;
struct IndexExpr;
struct AssignExpr;
struct CompoundAssignExpr;

struct ExprVisitor {
  virtual ~ExprVisitor() = default;
  virtual void visit(const IntLiteralExpr &node) = 0;
  virtual void visit(const FloatLiteralExpr &node) = 0;
  virtual void visit(const StringLiteralExpr &node) = 0;
  virtual void visit(const BoolLiteralExpr &node) = 0;
  virtual void visit(const NoneLiteralExpr &node) = 0;
  virtual void visit(const IdentifierExpr &node) = 0;
  virtual void visit(const BinaryOpExpr &node) = 0;
  virtual void visit(const UnaryOpExpr &node) = 0;
  virtual void visit(const CallExpr &node) = 0;
  virtual void visit(const MemberAccessExpr &node) = 0;
  virtual void visit(const IndexExpr &node) = 0;
  virtual void visit(const AssignExpr &node) = 0;
  virtual void visit(const CompoundAssignExpr &node) = 0;
};

enum class ExprKind : uint8_t {
  IntLiteral,
  FloatLiteral,
  StringLiteral,
  BoolLiteral,
  NoneLiteral,
  Identifier,
  BinaryOp,
  UnaryOp,
  Call,
  MemberAccess,
  Index,
  Assign,
  CompoundAssign,
};

struct Expr {
  ExprKind kind;
  SourceLocation location;

  Expr(ExprKind k, SourceLocation loc) : kind(k), location(loc) {}
  virtual ~Expr() = default;

  virtual void accept(ExprVisitor &visitor) const = 0;

  /// Pretty-print the expression
  virtual std::string toString() const;
};

struct IntLiteralExpr : Expr {
  int64_t value;

  IntLiteralExpr(int64_t v, SourceLocation loc)
      : Expr(ExprKind::IntLiteral, loc), value(v) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct FloatLiteralExpr : Expr {
  double value;

  FloatLiteralExpr(double v, SourceLocation loc)
      : Expr(ExprKind::FloatLiteral, loc), value(v) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct StringLiteralExpr : Expr {
  std::string value;

  StringLiteralExpr(std::string v, SourceLocation loc)
      : Expr(ExprKind::StringLiteral, loc), value(std::move(v)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct BoolLiteralExpr : Expr {
  bool value;

  BoolLiteralExpr(bool v, SourceLocation loc)
      : Expr(ExprKind::BoolLiteral, loc), value(v) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct NoneLiteralExpr : Expr {
  NoneLiteralExpr(SourceLocation loc) : Expr(ExprKind::NoneLiteral, loc) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct IdentifierExpr : Expr {
  std::string name;

  IdentifierExpr(std::string n, SourceLocation loc)
      : Expr(ExprKind::Identifier, loc), name(std::move(n)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

enum class BinaryOp : uint8_t {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Power,
  Eq,
  Neq,
  Lt,
  Lte,
  Gt,
  Gte,
  And,
  Or,
  BitwiseAnd,
  BitwiseOr,
  BitwiseXor,
};

struct BinaryOpExpr : Expr {
  BinaryOp op;
  ExpressionPointer lhs, rhs;

  BinaryOpExpr(BinaryOp op, ExpressionPointer lhs, ExpressionPointer rhs,
               SourceLocation loc)
      : Expr(ExprKind::BinaryOp, loc), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }

  static const char *opSymbol(BinaryOp op);
};

enum class UnaryOp : uint8_t {
  Negate,
  Not,
  BitwiseNot,
};

struct UnaryOpExpr : Expr {
  UnaryOp op;
  ExpressionPointer operand;

  UnaryOpExpr(UnaryOp op, ExpressionPointer operand, SourceLocation loc)
      : Expr(ExprKind::UnaryOp, loc), op(op), operand(std::move(operand)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct CallExpr : Expr {
  ExpressionPointer callee;
  std::vector<ExpressionPointer> args;

  CallExpr(ExpressionPointer callee, std::vector<ExpressionPointer> args,
           SourceLocation loc)
      : Expr(ExprKind::Call, loc), callee(std::move(callee)),
        args(std::move(args)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct MemberAccessExpr : Expr {
  ExpressionPointer object;
  std::string member;

  MemberAccessExpr(ExpressionPointer obj, std::string member,
                   SourceLocation loc)
      : Expr(ExprKind::MemberAccess, loc), object(std::move(obj)),
        member(std::move(member)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct IndexExpr : Expr {
  ExpressionPointer object;
  ExpressionPointer index;

  IndexExpr(ExpressionPointer obj, ExpressionPointer idx, SourceLocation loc)
      : Expr(ExprKind::Index, loc), object(std::move(obj)),
        index(std::move(idx)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

struct AssignExpr : Expr {
  ExpressionPointer target;
  ExpressionPointer value;

  AssignExpr(ExpressionPointer target, ExpressionPointer value,
             SourceLocation loc)
      : Expr(ExprKind::Assign, loc), target(std::move(target)),
        value(std::move(value)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

enum class CompoundAssignOp : uint8_t {
  AddAssign,
  SubAssign,
  MulAssign,
  DivAssign,
};

struct CompoundAssignExpr : Expr {
  CompoundAssignOp op;
  ExpressionPointer target;
  ExpressionPointer value;

  CompoundAssignExpr(CompoundAssignOp op, ExpressionPointer target,
                     ExpressionPointer value, SourceLocation loc)
      : Expr(ExprKind::CompoundAssign, loc), op(op), target(std::move(target)),
        value(std::move(value)) {}

  void accept(ExprVisitor &visitor) const override { visitor.visit(*this); }
};

// ============================================================
// Statements
// ============================================================

struct ExpressionStatement;
struct VariableDeclaration;
struct ReturnStatement;
struct Block;
struct IfStatement;
struct WhileStatement;
struct ForStatement;
struct BreakStatement;
struct ContinueStatement;
struct PassStatement;
struct DefDecl;
struct ClassDecl;
struct StructDecl;
struct AliasDecl;
struct ImportStmt;
struct AsmStmt;
struct TryExceptStmt;
struct RaiseStmt;

struct StatementVisitor {
  virtual ~StatementVisitor() = default;
  virtual void visit(const ExpressionStatement &node) = 0;
  virtual void visit(const VariableDeclaration &node) = 0;
  virtual void visit(const ReturnStatement &node) = 0;
  virtual void visit(const Block &node) = 0;
  virtual void visit(const IfStatement &node) = 0;
  virtual void visit(const WhileStatement &node) = 0;
  virtual void visit(const ForStatement &node) = 0;
  virtual void visit(const BreakStatement &node) = 0;
  virtual void visit(const ContinueStatement &node) = 0;
  virtual void visit(const PassStatement &node) = 0;
  virtual void visit(const DefDecl &node) = 0;
  virtual void visit(const ClassDecl &node) = 0;
  virtual void visit(const StructDecl &node) = 0;
  virtual void visit(const AliasDecl &node) = 0;
  virtual void visit(const ImportStmt &node) = 0;
  virtual void visit(const AsmStmt &node) = 0;
  virtual void visit(const TryExceptStmt &node) = 0;
  virtual void visit(const RaiseStmt &node) = 0;
};

enum class StatementKind : uint8_t {
  Expression,
  VarDecl,
  ReturnStmt,
  IfStmt,
  WhileStmt,
  ForStmt,
  BreakStmt,
  ContinueStmt,
  PassStmt,
  Block,
  DefDecl,
  ClassDecl,
  StructDecl,
  AliasDecl,
  ImportStmt,
  AsmStmt,
  TryExceptStmt,
  RaiseStmt,
};

enum class ArgumentConvention {
  None,
  Inout,
  Mut,
  Borrowed,
  Owned
};

/// A parameter in a function signature
struct Parameter {
  std::string name;
  TypePointer type;
  SourceLocation location;
  bool isPosOnly = false;     // /
  bool isKeywordOnly = false; // *
  bool isVariadic = false;    // *args
  bool isKeywordVariadic = false; // **kwargs
  ArgumentConvention convention = ArgumentConvention::None;
};

struct Statement {
  StatementKind kind;
  SourceLocation location;

  Statement(StatementKind k, SourceLocation loc) : kind(k), location(loc) {}
  virtual ~Statement() = default;

  virtual void accept(StatementVisitor &visitor) const = 0;

  /// Pretty-print with indentation
  virtual void dump(int indent = 0) const;
};

struct ExpressionStatement : Statement {
  ExpressionPointer expr;

  ExpressionStatement(ExpressionPointer e, SourceLocation loc)
      : Statement(StatementKind::Expression, loc), expr(std::move(e)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct VariableDeclaration : Statement {
  std::string name;
  TypePointer type;       // optional
  ExpressionPointer init; // optional
  bool isVar = false;     // Mojo 'var'
  bool isLet = false;     // Mojo 'let'

  VariableDeclaration(std::string name, TypePointer type,
                      ExpressionPointer init, SourceLocation loc,
                      bool isVar = false, bool isLet = false)
      : Statement(StatementKind::VarDecl, loc), name(std::move(name)),
        type(std::move(type)), init(std::move(init)), isVar(isVar),
        isLet(isLet) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct ReturnStatement : Statement {
  ExpressionPointer value; // optional

  ReturnStatement(ExpressionPointer value, SourceLocation loc)
      : Statement(StatementKind::ReturnStmt, loc), value(std::move(value)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct Block : Statement {
  std::vector<StmtPtr> statements;

  Block(std::vector<StmtPtr> stmts, SourceLocation loc)
      : Statement(StatementKind::Block, loc), statements(std::move(stmts)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct IfStatement : Statement {
  ExpressionPointer condition;
  std::unique_ptr<Block> thenBlock;
  // elif chains stored as nested IfStmt in elseBlock
  StmtPtr elseBlock; // Block or IfStmt (for elif)

  IfStatement(ExpressionPointer cond, std::unique_ptr<Block> then,
              StmtPtr elseB, SourceLocation loc)
      : Statement(StatementKind::IfStmt, loc), condition(std::move(cond)),
        thenBlock(std::move(then)), elseBlock(std::move(elseB)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct WhileStatement : Statement {
  ExpressionPointer condition;
  std::unique_ptr<Block> body;

  WhileStatement(ExpressionPointer cond, std::unique_ptr<Block> body,
                 SourceLocation loc)
      : Statement(StatementKind::WhileStmt, loc), condition(std::move(cond)),
        body(std::move(body)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct ForStatement : Statement {
  std::string varName;
  ExpressionPointer iterable;
  std::unique_ptr<Block> body;

  ForStatement(std::string var, ExpressionPointer iter,
               std::unique_ptr<Block> body, SourceLocation loc)
      : Statement(StatementKind::ForStmt, loc), varName(std::move(var)),
        iterable(std::move(iter)), body(std::move(body)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct BreakStatement : Statement {
  BreakStatement(SourceLocation loc)
      : Statement(StatementKind::BreakStmt, loc) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct ContinueStatement : Statement {
  ContinueStatement(SourceLocation loc)
      : Statement(StatementKind::ContinueStmt, loc) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct PassStatement : Statement {
  PassStatement(SourceLocation loc) : Statement(StatementKind::PassStmt, loc) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

/// Function declaration (Python-style def or Mojo-style fn)
struct DefDecl : Statement {
  std::string name;
  std::vector<Parameter> params;
  TypePointer returnType;
  std::unique_ptr<Block> body;
  bool isExtern;
  bool isFn; // Mojo 'fn' if true, 'def' if false
  bool raises = false;
  TypePointer raisesType;

  DefDecl(std::string name, std::vector<Parameter> params, TypePointer retType,
          std::unique_ptr<Block> body, SourceLocation loc, bool isExt = false,
          bool isFn = false, bool raises = false,
          TypePointer raisesType = nullptr)
      : Statement(StatementKind::DefDecl, loc), name(std::move(name)),
        params(std::move(params)), returnType(std::move(retType)),
        body(std::move(body)), isExtern(isExt), isFn(isFn), raises(raises),
        raisesType(std::move(raisesType)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct ClassDecl : Statement {
  std::string name;
  std::vector<StmtPtr> methods; // DefDecl methods

  ClassDecl(std::string name, std::vector<StmtPtr> methods, SourceLocation loc)
      : Statement(StatementKind::ClassDecl, loc), name(std::move(name)),
        methods(std::move(methods)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct StructDecl : Statement {
  std::string name;
  std::vector<StmtPtr> members;

  StructDecl(std::string name, std::vector<StmtPtr> members, SourceLocation loc)
      : Statement(StatementKind::StructDecl, loc), name(std::move(name)),
        members(std::move(members)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct AliasDecl : Statement {
  std::string name;
  ExpressionPointer value;

  AliasDecl(std::string name, ExpressionPointer value, SourceLocation loc)
      : Statement(StatementKind::AliasDecl, loc), name(std::move(name)),
        value(std::move(value)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct ImportStmt : Statement {
  std::string module;
  std::vector<std::string> names; // empty = import whole module
  std::string alias;              // for "as" imports

  ImportStmt(std::string module, std::vector<std::string> names,
             std::string alias, SourceLocation loc)
      : Statement(StatementKind::ImportStmt, loc), module(std::move(module)),
        names(std::move(names)), alias(std::move(alias)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct AsmStmt : Statement {
  std::string templateStr;
  std::string constraintsStr;
  std::vector<ExpressionPointer> args;
  bool isVolatile;

  AsmStmt(std::string templ, std::string constraints,
          std::vector<ExpressionPointer> args, bool isVol, SourceLocation loc)
      : Statement(StatementKind::AsmStmt, loc), templateStr(std::move(templ)),
        constraintsStr(std::move(constraints)), args(std::move(args)),
        isVolatile(isVol) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct TryExceptStmt : Statement {
  std::unique_ptr<Block> tryBlock;
  std::string exceptVar; // e
  std::unique_ptr<Block> exceptBlock;

  TryExceptStmt(std::unique_ptr<Block> tryB, std::string var,
                std::unique_ptr<Block> exceptB, SourceLocation loc)
      : Statement(StatementKind::TryExceptStmt, loc),
        tryBlock(std::move(tryB)), exceptVar(std::move(var)),
        exceptBlock(std::move(exceptB)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

struct RaiseStmt : Statement {
  ExpressionPointer expr;

  RaiseStmt(ExpressionPointer e, SourceLocation loc)
      : Statement(StatementKind::RaiseStmt, loc), expr(std::move(e)) {}

  void accept(StatementVisitor &visitor) const override {
    visitor.visit(*this);
  }
};

// ============================================================
// Module (top-level)
// ============================================================

struct Module {
  std::string filename;
  std::vector<StmtPtr> declarations;

  void dump() const;
};

} // namespace capybara
