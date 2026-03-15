#pragma once

#include "capybara/ast.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"

#include <unordered_map>
#include <unordered_set>

namespace capybara {

class Compiler : public ExprVisitor, public StatementVisitor {
  friend struct NamedTypeLowerer;

public:
  Compiler(std::unique_ptr<Module> module);
  ~Compiler();

  int compile();
  void setTargetTriple(const std::string &triple) { targetTriple_ = triple; }
  void enableStdLib() { useStdLib_ = true; }

  void compileModule(const Module &ast);

  // StatementVisitor
  void visit(const ExpressionStatement &node) override;
  void visit(const VariableDeclaration &node) override;
  void visit(const ReturnStatement &node) override;
  void visit(const Block &node) override;
  void visit(const IfStatement &node) override;
  void visit(const WhileStatement &node) override;
  void visit(const ForStatement &node) override;
  void visit(const BreakStatement &node) override;
  void visit(const ContinueStatement &node) override;
  void visit(const PassStatement &node) override;
  void visit(const DefDecl &node) override;
  void visit(const ClassDecl &node) override;
  void visit(const StructDecl &node) override;
  void visit(const AliasDecl &node) override;
  void visit(const ImportStmt &node) override;
  void visit(const AsmStmt &node) override;
  void visit(const TryExceptStmt &node) override;
  void visit(const RaiseStmt &node) override;

  // ExprVisitor
  void visit(const IntLiteralExpr &node) override;
  void visit(const FloatLiteralExpr &node) override;
  void visit(const StringLiteralExpr &node) override;
  void visit(const BoolLiteralExpr &node) override;
  void visit(const NoneLiteralExpr &node) override;
  void visit(const IdentifierExpr &node) override;
  void visit(const BinaryOpExpr &node) override;
  void visit(const UnaryOpExpr &node) override;
  void visit(const CallExpr &node) override;
  void visit(const MemberAccessExpr &node) override;
  void visit(const IndexExpr &node) override;
  void visit(const AssignExpr &node) override;
  void visit(const CompoundAssignExpr &node) override;

private:
  std::unique_ptr<Module> ast_;
  std::unique_ptr<llvm::LLVMContext> context_;
  std::unique_ptr<llvm::Module> module_;
  std::unique_ptr<llvm::IRBuilder<>> builder_;
  llvm::Value *lastExprValue_ = nullptr;
  llvm::Type *mapNamedTypeToLLVM(const TypeExpr *node);
  bool useStdLib_ = false;
  std::unordered_set<std::string> importedModules_;
  std::unordered_map<std::string, llvm::AllocaInst *> namedValues_;

  struct StructInfo {
    llvm::StructType *type;
    std::unordered_map<std::string, unsigned> memberIndices;
  };
  std::unordered_map<std::string, StructInfo> structs_;
  std::unordered_map<std::string, llvm::Value *> aliases_;

  std::string targetTriple_;
  void compileLibc(const std::string &triple);
  llvm::AllocaInst *createEntryBlockAlloca(llvm::Function *func,
                                           llvm::Type *allocType,
                                           const std::string &name);
  void addLocal(std::string name, llvm::AllocaInst *alloca, bool isMutable);
};

} // namespace capybara
