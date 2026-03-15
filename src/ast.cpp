#include "capybara/ast.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

namespace capybara {

// ── AST Printer (toString) ────────────────────────────────────

class ASTPrinter : public ExprVisitor, public TypeExprVisitor {
public:
  std::string result;

  void visit(const IntLiteralExpr &node) override {
    result = std::to_string(node.value);
  }
  void visit(const FloatLiteralExpr &node) override {
    std::ostringstream oss;
    oss << node.value;
    result = oss.str();
  }
  void visit(const StringLiteralExpr &node) override {
    result = "\"" + node.value + "\"";
  }
  void visit(const BoolLiteralExpr &node) override {
    result = node.value ? "True" : "False";
  }
  void visit(const NoneLiteralExpr &node) override { result = "None"; }
  void visit(const IdentifierExpr &node) override { result = node.name; }

  void visit(const BinaryOpExpr &node) override {
    node.lhs->accept(*this);
    std::string lhs = result;
    node.rhs->accept(*this);
    std::string rhs = result;
    result =
        "(" + lhs + " " + BinaryOpExpr::opSymbol(node.op) + " " + rhs + ")";
  }

  void visit(const UnaryOpExpr &node) override {
    const char *sym = "?";
    switch (node.op) {
    case UnaryOp::Negate:
      sym = "-";
      break;
    case UnaryOp::Not:
      sym = "not ";
      break;
    case UnaryOp::BitwiseNot:
      sym = "~";
      break;
    }
    node.operand->accept(*this);
    result = std::string(sym) + result;
  }

  void visit(const CallExpr &node) override {
    node.callee->accept(*this);
    std::string s = result + "(";
    for (size_t i = 0; i < node.args.size(); ++i) {
      if (i > 0)
        s += ", ";
      node.args[i]->accept(*this);
      s += result;
    }
    s += ")";
    result = s;
  }

  void visit(const MemberAccessExpr &node) override {
    node.object->accept(*this);
    result = result + "." + node.member;
  }

  void visit(const IndexExpr &node) override {
    node.object->accept(*this);
    std::string obj = result;
    node.index->accept(*this);
    result = obj + "[" + result + "]";
  }

  void visit(const AssignExpr &node) override {
    node.target->accept(*this);
    std::string target = result;
    node.value->accept(*this);
    result = target + " = " + result;
  }

  void visit(const CompoundAssignExpr &node) override {
    const char *sym = "?=";
    switch (node.op) {
    case CompoundAssignOp::AddAssign:
      sym = "+=";
      break;
    case CompoundAssignOp::SubAssign:
      sym = "-=";
      break;
    case CompoundAssignOp::MulAssign:
      sym = "*=";
      break;
    case CompoundAssignOp::DivAssign:
      sym = "/=";
      break;
    }
    node.target->accept(*this);
    std::string target = result;
    node.value->accept(*this);
    result = target + " " + sym + " " + result;
  }

  void visit(const NamedType &node) override { result = node.name; }
  void visit(const GenericType &node) override {
    std::string s = node.name + "[";
    for (size_t i = 0; i < node.typeArgs.size(); ++i) {
      if (i > 0)
        s += ", ";
      node.typeArgs[i]->accept(*this);
      s += result;
    }
    s += "]";
    result = s;
  }
};

std::string Expr::toString() const {
  ASTPrinter printer;
  this->accept(printer);
  return printer.result;
}

std::string TypeExpr::toString() const {
  ASTPrinter printer;
  this->accept(printer);
  return printer.result;
}

// ── AST Dumper (dump) ─────────────────────────────────────────

class ASTDumper : public StatementVisitor {
public:
  int indent = 0;

  void printIndent() {
    for (int i = 0; i < indent; ++i)
      llvm::outs() << "  ";
  }

  void visit(const ExpressionStatement &node) override {
    printIndent();
    llvm::outs() << "ExprStmt: " << node.expr->toString() << "\n";
  }

  void visit(const VariableDeclaration &node) override {
    printIndent();
    if (node.isVar)
      llvm::outs() << "VarDecl (var): " << node.name;
    else if (node.isLet)
      llvm::outs() << "VarDecl (let): " << node.name;
    else
      llvm::outs() << "VarDecl: " << node.name;

    if (node.type)
      llvm::outs() << " : " << node.type->toString();
    if (node.init)
      llvm::outs() << " = " << node.init->toString();
    llvm::outs() << "\n";
  }

  void visit(const ReturnStatement &node) override {
    printIndent();
    llvm::outs() << "Return";
    if (node.value)
      llvm::outs() << ": " << node.value->toString();
    llvm::outs() << "\n";
  }

  void visit(const Block &node) override {
    printIndent();
    llvm::outs() << "Block:\n";
    indent++;
    for (auto &stmt : node.statements) {
      stmt->accept(*this);
    }
    indent--;
  }

  void visit(const IfStatement &node) override {
    printIndent();
    llvm::outs() << "If: " << node.condition->toString() << "\n";
    indent++;
    node.thenBlock->accept(*this);
    indent--;
    if (node.elseBlock) {
      printIndent();
      llvm::outs() << "Else:\n";
      indent++;
      node.elseBlock->accept(*this);
      indent--;
    }
  }

  void visit(const WhileStatement &node) override {
    printIndent();
    llvm::outs() << "While: " << node.condition->toString() << "\n";
    indent++;
    node.body->accept(*this);
    indent--;
  }

  void visit(const ForStatement &node) override {
    printIndent();
    llvm::outs() << "For: " << node.varName << " in "
                 << node.iterable->toString() << "\n";
    indent++;
    node.body->accept(*this);
    indent--;
  }

  void visit(const BreakStatement &node) override {
    printIndent();
    llvm::outs() << "Break\n";
  }

  void visit(const ContinueStatement &node) override {
    printIndent();
    llvm::outs() << "Continue\n";
  }

  void visit(const PassStatement &node) override {
    printIndent();
    llvm::outs() << "Pass\n";
  }

  void visit(const DefDecl &node) override {
    printIndent();
    llvm::outs() << (node.isFn ? "FnDecl: " : "DefDecl: ") << node.name << "(";
    for (size_t i = 0; i < node.params.size(); ++i) {
      if (i > 0)
        llvm::outs() << ", ";
      const auto &p = node.params[i];
      if (p.isVariadic)
        llvm::outs() << "*";
      if (p.isKeywordVariadic)
        llvm::outs() << "**";
      llvm::outs() << p.name;
      if (p.type)
        llvm::outs() << ": " << p.type->toString();
      if (p.isPosOnly)
        llvm::outs() << " (pos-only)";
      if (p.isKeywordOnly)
        llvm::outs() << " (kw-only)";
      if (p.convention == ArgumentConvention::Inout)
        llvm::outs() << " (inout)";
      else if (p.convention == ArgumentConvention::Mut)
        llvm::outs() << " (mut)";
      else if (p.convention == ArgumentConvention::Borrowed)
        llvm::outs() << " (borrowed)";
      else if (p.convention == ArgumentConvention::Owned)
        llvm::outs() << " (owned)";
    }
    llvm::outs() << ")";
    if (node.returnType)
      llvm::outs() << " -> " << node.returnType->toString();
    if (node.raises) {
      llvm::outs() << " raises";
      if (node.raisesType)
        llvm::outs() << " " << node.raisesType->toString();
    }
    llvm::outs() << "\n";
    if (node.body) {
      indent++;
      node.body->accept(*this);
      indent--;
    }
  }

  void visit(const ClassDecl &node) override {
    printIndent();
    llvm::outs() << "ClassDecl: " << node.name << "\n";
    indent++;
    for (auto &method : node.methods) {
      method->accept(*this);
    }
    indent--;
  }

  void visit(const StructDecl &node) override {
    printIndent();
    llvm::outs() << "StructDecl: " << node.name << "\n";
    indent++;
    for (auto &member : node.members) {
      member->accept(*this);
    }
    indent--;
  }

  void visit(const AliasDecl &node) override {
    printIndent();
    llvm::outs() << "Alias: " << node.name << " = " << node.value->toString()
                 << "\n";
  }

  void visit(const ImportStmt &node) override {
    printIndent();
    if (node.names.empty()) {
      llvm::outs() << "Import: " << node.module;
    } else {
      llvm::outs() << "FromImport: " << node.module << " import ";
      for (size_t i = 0; i < node.names.size(); ++i) {
        if (i > 0)
          llvm::outs() << ", ";
        llvm::outs() << node.names[i];
      }
    }
    if (!node.alias.empty())
      llvm::outs() << " as " << node.alias;
    llvm::outs() << "\n";
  }

  void visit(const AsmStmt &node) override {
    printIndent();
    llvm::outs() << "Asm: \"" << node.templateStr << "\" constraints=\""
                 << node.constraintsStr << "\"";
    if (node.isVolatile)
      llvm::outs() << " (volatile)";
    llvm::outs() << "\n";
    for (const auto &arg : node.args) {
      printIndent();
      llvm::outs() << "  Arg: " << arg->toString() << "\n";
    }
  }

  void visit(const TryExceptStmt &node) override {
    printIndent();
    llvm::outs() << "Try:\n";
    indent++;
    if (node.tryBlock)
      node.tryBlock->accept(*this);
    indent--;
    printIndent();
    llvm::outs() << "Except";
    if (!node.exceptVar.empty())
      llvm::outs() << " " << node.exceptVar;
    llvm::outs() << ":\n";
    indent++;
    if (node.exceptBlock)
      node.exceptBlock->accept(*this);
    indent--;
  }

  void visit(const RaiseStmt &node) override {
    printIndent();
    llvm::outs() << "Raise";
    if (node.expr)
      llvm::outs() << ": " << node.expr->toString();
    llvm::outs() << "\n";
  }
};

void Statement::dump(int indent) const {
  ASTDumper dumper;
  dumper.indent = indent;
  this->accept(dumper);
}

// ── Helpers ───────────────────────────────────────────────────

const char *BinaryOpExpr::opSymbol(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add:
    return "+";
  case BinaryOp::Sub:
    return "-";
  case BinaryOp::Mul:
    return "*";
  case BinaryOp::Div:
    return "/";
  case BinaryOp::Mod:
    return "%";
  case BinaryOp::Power:
    return "**";
  case BinaryOp::Eq:
    return "==";
  case BinaryOp::Neq:
    return "!=";
  case BinaryOp::Lt:
    return "<";
  case BinaryOp::Lte:
    return "<=";
  case BinaryOp::Gt:
    return ">";
  case BinaryOp::Gte:
    return ">=";
  case BinaryOp::And:
    return "and";
  case BinaryOp::Or:
    return "or";
  case BinaryOp::BitwiseAnd:
    return "&";
  case BinaryOp::BitwiseOr:
    return "|";
  case BinaryOp::BitwiseXor:
    return "^";
  }
  return "??";
}

// ── Module ────────────────────────────────────────────────────

void Module::dump() const {
  llvm::outs() << "Module: " << filename << "\n";
  ASTDumper dumper;
  dumper.indent = 1;
  for (auto &decl : declarations) {
    decl->accept(dumper);
  }
}

} // namespace capybara
