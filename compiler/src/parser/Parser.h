#pragma once

#include "parser/AST.h"
#include "common/Diagnostics.h"
#include "common/Token.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace xb {

// Hand-written recursive descent parser over the preprocessor's output.
// Keywords are not reserved: they are recognised by position (statement
// start) and may be abbreviated, as in Clipper (FUNC, PROC, RETU, ENDD, ...).
// Commands such as ?, DO WHILE, STORE, QUIT are NOT handled here: they are
// #command rules in include/std.ch.
class Parser {
public:
  Parser(std::vector<Token> toks, Diagnostics &diag) : toks_(std::move(toks)), diag_(diag) {}
  std::unique_ptr<Module> parseModule(const std::string &moduleName);

private:
  // Parser.cpp: token access
  const Token &cur() const { return toks_[pos_]; }
  const Token &ahead(size_t n = 1) const;
  bool at(Tok k) const { return cur().kind == k; }
  bool accept(Tok k);
  bool expect(Tok k, const char *what);
  std::string found() const;
  bool isAssignAfterEach() const;
  void advance() { if (pos_ + 1 < toks_.size()) ++pos_; }
  bool atEndOfStmt() const { return at(Tok::EndOfStmt) || at(Tok::Eof); }
  void endOfStmt();
  void skipToEndOfStmt();
  void skipBlankStmts();

  // Parser.cpp: keywords
  bool isKw(const Token &t, const char *kw) const;
  bool atKw(const char *kw) const { return isKw(cur(), kw); }
  bool atStmtKw(const char *kw) const;
  bool atFuncStart() const;
  bool atInlineIf() const;
  bool atBlockEnd(std::initializer_list<const char *> enders) const;

  // Parser.cpp: functions and blocks
  std::unique_ptr<FuncDecl> parseFunc();
  Block parseBlock(std::initializer_list<const char *> enders);
  void closeBlock(std::initializer_list<const char *> closers, const char *opener, SourceLoc openLoc);

  // Statements.cpp
  StmtPtr parseStmt();
  StmtPtr parseDecl(StmtKind kind);
  StmtPtr parseDoCase();
  StmtPtr parseSwitch();
  StmtPtr parseForEach();
  void rejectBeforeFirstCase(const char *construct, std::initializer_list<const char *> starts);
  StmtPtr parseIf();
  StmtPtr parseWhile();
  StmtPtr parseFor();

  // Expressions.cpp
  ExprPtr parseExpr();
  ExprPtr parseAssign();
  ExprPtr parseOr();
  ExprPtr parseAnd();
  ExprPtr parseNot();
  ExprPtr parseEquality();
  ExprPtr parseRelational();
  ExprPtr parseAdditive();
  ExprPtr parseMultiplicative();
  ExprPtr parsePower();
  ExprPtr parseUnary();
  ExprPtr parsePostfix();
  ExprPtr parsePrimary();
  ExprPtr parseBraced();
  std::vector<ExprPtr> parseArgs(Tok close);

  std::vector<Token> toks_;
  Diagnostics &diag_;
  size_t pos_ = 0;
};

} // namespace xb
