#pragma once

#include "parser/AST.h"
#include "common/Diagnostics.h"
#include "common/Token.h"

#include <initializer_list>
#include <memory>
#include <optional>
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
  // Nesting limits: deeper sources are reported instead of exhausting the
  // compiler's stack (every later stage recurses over the tree too).
  static constexpr unsigned kMaxExprNesting = 1024;  // parentheses, calls, arrays, blocks, prefix operators
  static constexpr unsigned kMaxBlockNesting = 1024; // IF, DO CASE, loops, ... inside each other

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
  void advance() {
    if (at(Tok::EndOfStmt)) quietStmt_ = false; // the statement a nesting error silenced ends
    if (pos_ + 1 < toks_.size()) ++pos_;
  }
  bool atEndOfStmt() const { return at(Tok::EndOfStmt) || at(Tok::Eof); }
  void endOfStmt();
  void skipToEndOfStmt();
  void skipBlankStmts();
  void tooDeep(const char *what, unsigned limit);

  // Parser.cpp: keywords
  bool isKw(const Token &t, const char *kw) const;
  bool atKw(const char *kw) const { return isKw(cur(), kw); }
  bool atStmtKw(const char *kw) const;
  bool atFuncStart() const;
  bool atInlineIf() const;
  bool atBlockEnd(std::initializer_list<const char *> enders) const;
  bool atExtern() const;
  bool atLinkDecl() const;

  // Parser.cpp: functions, declarations and blocks
  std::unique_ptr<FuncDecl> parseFunc();
  std::optional<ExternDecl> parseExtern();
  void parseLinkDecl();
  void finishModule(Module &mod);
  Block parseBlock(std::initializer_list<const char *> enders);
  void closeBlock(std::initializer_list<const char *> closers, const char *opener, SourceLoc openLoc);

  // Statements.cpp
  StmtPtr parseStmt();
  StmtPtr parseStmtKind();
  uint32_t stmtEndLine() const;
  StmtPtr parseDecl(StmtKind kind);
  StmtPtr parseDoCase();
  StmtPtr parseSwitch();
  StmtPtr parseForEach();
  void rejectBeforeFirstCase(const char *construct, std::initializer_list<const char *> starts);
  StmtPtr parseIf();
  StmtPtr parseWhile();
  StmtPtr parseSequence();
  StmtPtr parseFor();

  // Expressions.cpp
  ExprPtr parseExpr();
  ExprPtr parseAssign();
  ExprPtr parseOr();
  ExprPtr parseAnd();
  ExprPtr parseNot();
  ExprPtr parseBinary(size_t level);
  bool enterExpr();
  ExprPtr parseUnary();
  ExprPtr parsePostfix();
  ExprPtr parsePrimary();
  ExprPtr parseBraced();
  std::vector<ExprPtr> parseArgs(Tok close, bool refs, bool spread);
  ExprPtr parseRef();

  std::vector<Token> toks_;
  Diagnostics &diag_;
  size_t pos_ = 0;
  unsigned exprDepth_ = 0;
  unsigned blockDepth_ = 0;
  // After a nesting error the parser reports nothing more until the statement
  // ends (expressions: until advance() passes an end of statement) or at all
  // (blocks: the rest of the source is skipped), so the closers it then misses
  // are not reported one by one.
  bool quietStmt_ = false;
  bool stopped_ = false;
  // ANNOUNCE and EXTERNAL anywhere in the module, given to it at its end.
  std::optional<Request> announce_;
  std::vector<Request> requests_;
  bool quiet() const { return quietStmt_ || stopped_; }
};

} // namespace xb
