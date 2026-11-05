#include "Parser.h"

#include "Text.h"

#include <cstring>

namespace xb {

const Token &Parser::ahead(size_t n) const {
  size_t p = pos_ + n;
  return p < toks_.size() ? toks_[p] : toks_.back();
}

bool Parser::accept(Tok k) {
  if (!at(k)) return false;
  advance();
  return true;
}

bool Parser::expect(Tok k, const char *what) {
  if (accept(k)) return true;
  diag_.error(cur().loc, std::string("expected ") + what + ", found " +
                             (at(Tok::Ident) ? "'" + cur().text + "'" : std::string(tokName(cur().kind))));
  return false;
}

void Parser::skipToEndOfStmt() {
  while (!atEndOfStmt()) advance();
}

void Parser::endOfStmt() {
  if (!atEndOfStmt()) {
    diag_.error(cur().loc, std::string("unexpected ") +
                               (at(Tok::Ident) ? "'" + cur().text + "'" : std::string(tokName(cur().kind))));
    skipToEndOfStmt();
  }
  accept(Tok::EndOfStmt);
}

void Parser::skipBlankStmts() {
  while (accept(Tok::EndOfStmt)) {}
}

// Keyword match. Keywords may be abbreviated down to a minimum length
// (Clipper rules, e.g. FUNC, PROC, RETU, ENDI, ELSEI).
bool Parser::isKw(const Token &t, const char *kw) const {
  struct Kw {
    const char *name;
    size_t minLen;
  };
  static const Kw table[] = {
      {"DO", 2},     {"ELSE", 4},  {"ELSEIF", 5}, {"END", 3},     {"ENDDO", 4},  {"ENDIF", 4},
      {"EXIT", 4},   {"FOR", 3},   {"FUNCTION", 4}, {"IF", 2},    {"IIF", 3},    {"LOCAL", 4},
      {"LOOP", 4},   {"NEXT", 4},  {"NIL", 3},    {"PROCEDURE", 4}, {"RETURN", 4}, {"STATIC", 4},
      {"STEP", 4},   {"TO", 2},    {"WHILE", 4},  {"WITH", 4},    {"MEMVAR", 4}, {"FIELD", 4},
      {"PRIVATE", 4}, {"PUBLIC", 4}, {"PARAMETERS", 4},
  };
  if (t.kind != Tok::Ident) return false;
  size_t minLen = std::strlen(kw);
  for (const Kw &k : table)
    if (std::strcmp(k.name, kw) == 0) minLen = k.minLen;
  return keywordMatches(t.upper, kw, minLen);
}

// A keyword at statement start - unless the "keyword" is really a variable
// (`next := 1`, `exit = 2`, `end[1]`). Keywords that form a statement on
// their own are keywords only when nothing follows them, and FOR only when
// its statement has a TO.
bool Parser::atStmtKw(const char *kw) const {
  if (!atKw(kw)) return false;
  switch (ahead().kind) {
  case Tok::Assign: case Tok::PlusEq: case Tok::MinusEq: case Tok::MulEq: case Tok::DivEq:
  case Tok::ModEq: case Tok::PowEq: case Tok::Inc: case Tok::Dec: case Tok::Arrow:
  case Tok::Colon: case Tok::LBracket: case Tok::Eq:
    return false;
  default:
    break;
  }
  static const char *const standalone[] = {"EXIT", "LOOP", "NEXT", "END", "ENDIF", "ENDDO", "ELSE"};
  for (const char *s : standalone)
    if (std::strcmp(s, kw) == 0) return ahead().kind == Tok::EndOfStmt || ahead().kind == Tok::Eof;
  if (std::strcmp(kw, "FOR") == 0) {
    for (size_t p = pos_ + 1; p < toks_.size() && toks_[p].kind != Tok::EndOfStmt && toks_[p].kind != Tok::Eof; ++p)
      if (isKw(toks_[p], "TO")) return true;
    return false;
  }
  return true;
}

// IF( cond, a, b ) used as a statement is an inline IF, not an IF block.
bool Parser::atInlineIf() const {
  if (ahead().kind != Tok::LParen) return false;
  int depth = 0, commas = 0;
  for (size_t p = pos_ + 1; p < toks_.size(); ++p) {
    Tok k = toks_[p].kind;
    if (k == Tok::EndOfStmt || k == Tok::Eof) return false;
    if (k == Tok::LParen || k == Tok::LBracket || k == Tok::LBrace) {
      ++depth;
    } else if (k == Tok::RParen || k == Tok::RBracket || k == Tok::RBrace) {
      if (--depth == 0) {
        Tok after = p + 1 < toks_.size() ? toks_[p + 1].kind : Tok::Eof;
        return commas == 2 && (after == Tok::EndOfStmt || after == Tok::Eof);
      }
    } else if (k == Tok::Comma && depth == 1) {
      ++commas;
    }
  }
  return false;
}

bool Parser::atFuncStart() const {
  if (atStmtKw("FUNCTION") || atStmtKw("PROCEDURE")) return true;
  return atStmtKw("STATIC") && (isKw(ahead(), "FUNCTION") || isKw(ahead(), "PROCEDURE"));
}

bool Parser::atBlockEnd(std::initializer_list<const char *> enders) const {
  for (const char *e : enders)
    if (atStmtKw(e)) return true;
  return false;
}

std::unique_ptr<Module> Parser::parseModule(const std::string &moduleName) {
  auto mod = std::make_unique<Module>();
  mod->name = moduleName;
  skipBlankStmts();
  if (!at(Tok::Eof) && !atFuncStart()) {
    // Statements before the first FUNCTION/PROCEDURE form an implicit
    // procedure named after the module.
    auto fn = std::make_unique<FuncDecl>();
    fn->loc = cur().loc;
    fn->name = moduleName;
    fn->isProcedure = true;
    fn->implicit = true;
    fn->body = parseBlock({});
    mod->funcs.push_back(std::move(fn));
  }
  while (!at(Tok::Eof) && !diag_.limitReached()) {
    skipBlankStmts();
    if (at(Tok::Eof)) break;
    if (!atFuncStart()) { // only reachable after an error
      diag_.error(cur().loc, "statement outside of a function");
      skipToEndOfStmt();
      continue;
    }
    mod->funcs.push_back(parseFunc());
  }
  return mod;
}

std::unique_ptr<FuncDecl> Parser::parseFunc() {
  auto fn = std::make_unique<FuncDecl>();
  fn->loc = cur().loc;
  if (atKw("STATIC")) {
    fn->isStatic = true;
    advance();
  }
  fn->isProcedure = atKw("PROCEDURE");
  advance();
  if (at(Tok::Ident)) {
    fn->name = cur().upper;
    advance();
  } else {
    expect(Tok::Ident, "function name");
  }
  if (accept(Tok::LParen)) {
    if (!at(Tok::RParen)) {
      do {
        if (at(Tok::Ident)) {
          fn->params.push_back({cur().loc, cur().upper});
          advance();
        } else {
          expect(Tok::Ident, "parameter name");
          break;
        }
      } while (accept(Tok::Comma));
    }
    expect(Tok::RParen, "')'");
  }
  endOfStmt();
  fn->body = parseBlock({});
  return fn;
}

Block Parser::parseBlock(std::initializer_list<const char *> enders) {
  Block b;
  while (true) {
    skipBlankStmts();
    if (at(Tok::Eof) || diag_.limitReached() || atFuncStart() || atBlockEnd(enders)) break;
    size_t before = pos_;
    if (StmtPtr s = parseStmt()) b.push_back(std::move(s));
    if (pos_ == before) advance(); // guarantee progress after errors
  }
  return b;
}

void Parser::closeBlock(std::initializer_list<const char *> closers, const char *opener, SourceLoc openLoc) {
  if (atBlockEnd(closers)) {
    advance();
    endOfStmt();
    return;
  }
  diag_.error(openLoc, std::string(opener) + " without matching " + *closers.begin());
}

StmtPtr Parser::parseStmt() {
  SourceLoc loc = cur().loc;

  if (atStmtKw("LOCAL")) return parseLocal();
  if (atStmtKw("IF") && !atInlineIf()) return parseIf();
  if (atStmtKw("WHILE")) return parseWhile();
  if (atStmtKw("FOR")) return parseFor();
  if (atStmtKw("RETURN")) {
    advance();
    ExprPtr v;
    if (!atEndOfStmt()) v = parseExpr();
    endOfStmt();
    return std::make_unique<ReturnStmt>(loc, std::move(v));
  }
  if (atStmtKw("EXIT") || atStmtKw("LOOP")) {
    StmtKind k = atKw("EXIT") ? StmtKind::Exit : StmtKind::Loop;
    advance();
    endOfStmt();
    return std::make_unique<JumpStmt>(loc, k);
  }
  if (atStmtKw("DO") && ahead().kind == Tok::Ident) { // DO proc [WITH args]
    advance();
    auto call = std::make_unique<CallExpr>(cur().loc, cur().upper);
    advance();
    if (atKw("WITH")) { // empty arguments pass NIL, as in a call
      advance();
      do {
        if (at(Tok::Comma) || atEndOfStmt()) call->args.push_back(std::make_unique<NilExpr>(cur().loc));
        else call->args.push_back(parseExpr());
      } while (accept(Tok::Comma));
    }
    endOfStmt();
    return std::make_unique<ExprStmt>(loc, std::move(call));
  }
  for (const char *closer : {"ENDIF", "ENDDO", "NEXT", "END", "ELSE", "ELSEIF"}) {
    if (atStmtKw(closer)) {
      diag_.error(loc, std::string(closer) + " does not match any open block");
      skipToEndOfStmt();
      return nullptr;
    }
  }
  if (atStmtKw("STATIC") || atStmtKw("PRIVATE") || atStmtKw("PUBLIC") || atStmtKw("MEMVAR") ||
      atStmtKw("FIELD") || atStmtKw("PARAMETERS")) {
    diag_.error(loc, "'" + cur().text + "' is not supported yet");
    skipToEndOfStmt();
    return nullptr;
  }

  // Statement-level `x = expr` is an assignment, not a comparison.
  if (at(Tok::Ident) && ahead().kind == Tok::Eq) {
    auto target = std::make_unique<VarExpr>(cur().loc, cur().upper);
    advance();
    SourceLoc aloc = cur().loc;
    advance();
    ExprPtr value = parseExpr();
    endOfStmt();
    return std::make_unique<ExprStmt>(loc, std::make_unique<AssignExpr>(aloc, std::move(target), std::move(value)));
  }

  ExprPtr e = parseExpr();
  switch (e->kind) {
  case ExprKind::Call: case ExprKind::Assign: case ExprKind::IncDec: case ExprKind::IIf:
    break;
  default:
    diag_.warning(e->loc, "meaningless use of expression");
  }
  endOfStmt();
  return std::make_unique<ExprStmt>(loc, std::move(e));
}

StmtPtr Parser::parseLocal() {
  auto st = std::make_unique<LocalStmt>(cur().loc);
  advance();
  do {
    if (!at(Tok::Ident)) {
      expect(Tok::Ident, "variable name");
      skipToEndOfStmt();
      break;
    }
    LocalDecl d;
    d.loc = cur().loc;
    d.name = cur().upper;
    advance();
    if (accept(Tok::Assign)) d.init = parseExpr();
    st->decls.push_back(std::move(d));
  } while (accept(Tok::Comma));
  endOfStmt();
  return st;
}

StmtPtr Parser::parseIf() {
  auto st = std::make_unique<IfStmt>(cur().loc);
  SourceLoc openLoc = cur().loc;
  advance();
  IfStmt::Arm first;
  first.cond = parseExpr();
  endOfStmt();
  first.body = parseBlock({"ELSEIF", "ELSE", "ENDIF", "END"});
  st->arms.push_back(std::move(first));
  while (atStmtKw("ELSEIF")) {
    advance();
    IfStmt::Arm arm;
    arm.cond = parseExpr();
    endOfStmt();
    arm.body = parseBlock({"ELSEIF", "ELSE", "ENDIF", "END"});
    st->arms.push_back(std::move(arm));
  }
  if (atStmtKw("ELSE")) {
    advance();
    endOfStmt();
    st->elseBody = parseBlock({"ENDIF", "END"});
  }
  closeBlock({"ENDIF", "END"}, "IF", openLoc);
  return st;
}

StmtPtr Parser::parseWhile() {
  SourceLoc openLoc = cur().loc;
  advance(); // WHILE
  auto st = std::make_unique<WhileStmt>(openLoc, parseExpr());
  endOfStmt();
  st->body = parseBlock({"ENDDO", "END"});
  closeBlock({"ENDDO", "END"}, "WHILE", openLoc);
  return st;
}

StmtPtr Parser::parseFor() {
  SourceLoc openLoc = cur().loc;
  auto st = std::make_unique<ForStmt>(openLoc);
  advance();
  if (!at(Tok::Ident)) {
    expect(Tok::Ident, "loop variable");
    skipToEndOfStmt();
    return nullptr;
  }
  st->var = std::make_unique<VarExpr>(cur().loc, cur().upper);
  advance();
  if (!accept(Tok::Assign) && !accept(Tok::Eq)) expect(Tok::Assign, "':='");
  st->start = parseExpr();
  if (atKw("TO")) advance();
  else expect(Tok::Ident, "TO");
  st->limit = parseExpr();
  if (atKw("STEP")) {
    advance();
    st->step = parseExpr();
  }
  endOfStmt();
  st->body = parseBlock({"NEXT", "END"});
  closeBlock({"NEXT", "END"}, "FOR", openLoc);
  return st;
}

ExprPtr Parser::parseExpr() { return parseAssign(); }

static bool isLvalue(const Expr &e) { return e.kind == ExprKind::Var; }

ExprPtr Parser::parseAssign() {
  ExprPtr lhs = parseOr();
  BinOp op = BinOp::Add;
  bool compound = true;
  switch (cur().kind) {
  case Tok::Assign: compound = false; break;
  case Tok::PlusEq: op = BinOp::Add; break;
  case Tok::MinusEq: op = BinOp::Sub; break;
  case Tok::MulEq: op = BinOp::Mul; break;
  case Tok::DivEq: op = BinOp::Div; break;
  case Tok::ModEq: op = BinOp::Mod; break;
  case Tok::PowEq: op = BinOp::Pow; break;
  default: return lhs;
  }
  SourceLoc loc = cur().loc;
  advance();
  if (!isLvalue(*lhs)) diag_.error(lhs->loc, "invalid assignment target");
  ExprPtr rhs = parseAssign(); // right associative
  auto a = std::make_unique<AssignExpr>(loc, std::move(lhs), std::move(rhs));
  a->compound = compound;
  a->op = op;
  return a;
}

ExprPtr Parser::parseOr() {
  ExprPtr lhs = parseAnd();
  while (at(Tok::Or)) {
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<LogicExpr>(loc, ExprKind::Or, std::move(lhs), parseAnd());
  }
  return lhs;
}

ExprPtr Parser::parseAnd() {
  ExprPtr lhs = parseNot();
  while (at(Tok::And)) {
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<LogicExpr>(loc, ExprKind::And, std::move(lhs), parseNot());
  }
  return lhs;
}

ExprPtr Parser::parseNot() {
  if (at(Tok::Not)) {
    SourceLoc loc = cur().loc;
    advance();
    return std::make_unique<UnaryExpr>(loc, UnOp::Not, parseNot());
  }
  return parseEquality();
}

ExprPtr Parser::parseEquality() {
  ExprPtr lhs = parseRelational();
  while (true) {
    BinOp op;
    switch (cur().kind) {
    case Tok::Eq: op = BinOp::Eq; break;
    case Tok::ExactEq: op = BinOp::ExactEq; break;
    case Tok::NotEq: op = BinOp::NotEq; break;
    default: return lhs;
    }
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parseRelational());
  }
}

ExprPtr Parser::parseRelational() {
  ExprPtr lhs = parseAdditive();
  while (true) {
    BinOp op;
    switch (cur().kind) {
    case Tok::Lt: op = BinOp::Lt; break;
    case Tok::Le: op = BinOp::Le; break;
    case Tok::Gt: op = BinOp::Gt; break;
    case Tok::Ge: op = BinOp::Ge; break;
    case Tok::Dollar: op = BinOp::In; break;
    default: return lhs;
    }
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parseAdditive());
  }
}

ExprPtr Parser::parseAdditive() {
  ExprPtr lhs = parseMultiplicative();
  while (at(Tok::Plus) || at(Tok::Minus)) {
    BinOp op = at(Tok::Plus) ? BinOp::Add : BinOp::Sub;
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parseMultiplicative());
  }
  return lhs;
}

ExprPtr Parser::parseMultiplicative() {
  ExprPtr lhs = parsePower();
  while (at(Tok::Star) || at(Tok::Slash) || at(Tok::Percent)) {
    BinOp op = at(Tok::Star) ? BinOp::Mul : at(Tok::Slash) ? BinOp::Div : BinOp::Mod;
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parsePower());
  }
  return lhs;
}

ExprPtr Parser::parsePower() {
  ExprPtr lhs = parseUnary();
  while (at(Tok::Power)) {
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, BinOp::Pow, std::move(lhs), parseUnary());
  }
  return lhs;
}

ExprPtr Parser::parseUnary() {
  SourceLoc loc = cur().loc;
  if (accept(Tok::Minus)) return std::make_unique<UnaryExpr>(loc, UnOp::Neg, parseUnary());
  if (accept(Tok::Plus)) return std::make_unique<UnaryExpr>(loc, UnOp::Plus, parseUnary());
  if (at(Tok::Inc) || at(Tok::Dec)) {
    bool inc = at(Tok::Inc);
    advance();
    ExprPtr t = parseUnary();
    if (!isLvalue(*t)) diag_.error(t->loc, "invalid operand for ++/--");
    return std::make_unique<IncDecExpr>(loc, inc, true, std::move(t));
  }
  return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
  ExprPtr e = parsePrimary();
  while (at(Tok::Inc) || at(Tok::Dec)) {
    bool inc = at(Tok::Inc);
    SourceLoc loc = cur().loc;
    advance();
    if (!isLvalue(*e)) diag_.error(e->loc, "invalid operand for ++/--");
    e = std::make_unique<IncDecExpr>(loc, inc, false, std::move(e));
  }
  return e;
}

std::vector<ExprPtr> Parser::parseArgs(Tok close) {
  // Empty arguments - f( a, , b ) - are passed as NIL.
  std::vector<ExprPtr> args;
  if (at(close)) return args;
  while (true) {
    if (at(Tok::Comma) || at(close)) args.push_back(std::make_unique<NilExpr>(cur().loc));
    else args.push_back(parseExpr());
    if (!accept(Tok::Comma)) break;
  }
  return args;
}

ExprPtr Parser::parsePrimary() {
  const Token &t = cur();
  SourceLoc loc = t.loc;
  switch (t.kind) {
  case Tok::Number: {
    auto e = std::make_unique<NumberExpr>(loc, t.isInt, t.ival, t.dval, t.dec, t.width);
    advance();
    return e;
  }
  case Tok::String: {
    auto e = std::make_unique<StringExpr>(loc, t.text);
    advance();
    return e;
  }
  case Tok::True: advance(); return std::make_unique<LogicalExpr>(loc, true);
  case Tok::False: advance(); return std::make_unique<LogicalExpr>(loc, false);
  case Tok::LParen: {
    advance();
    ExprPtr e = parseExpr();
    expect(Tok::RParen, "')'");
    return e;
  }
  case Tok::Ident: {
    std::string name = t.upper;
    if (ahead().kind == Tok::LParen) {
      advance();
      advance();
      if (name == "IIF" || name == "IF") {
        auto args = parseArgs(Tok::RParen);
        expect(Tok::RParen, "')'");
        if (args.size() != 3) {
          diag_.error(loc, name + "() requires exactly 3 arguments");
          return std::make_unique<NilExpr>(loc);
        }
        return std::make_unique<IIfExpr>(loc, std::move(args[0]), std::move(args[1]), std::move(args[2]));
      }
      auto call = std::make_unique<CallExpr>(loc, name);
      call->args = parseArgs(Tok::RParen);
      expect(Tok::RParen, "')'");
      return call;
    }
    advance();
    if (name == "NIL") return std::make_unique<NilExpr>(loc);
    return std::make_unique<VarExpr>(loc, name);
  }
  default:
    diag_.error(loc, std::string("expected expression, found ") + tokName(t.kind));
    if (!atEndOfStmt()) advance();
    return std::make_unique<NilExpr>(loc);
  }
}

} // namespace xb
