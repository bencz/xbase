// Token access, keyword recognition, the module and its functions, statement blocks.
#include "parser/Parser.h"

#include "common/Text.h"

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

// "found 'name'" or "found <token kind>", for messages about the current token.
std::string Parser::found() const {
  return "found " + (at(Tok::Ident) ? "'" + cur().text + "'" : std::string(tokName(cur().kind)));
}

bool Parser::expect(Tok k, const char *what) {
  if (accept(k)) return true;
  diag_.error(cur().loc, std::string("expected ") + what + ", " + found());
  return false;
}

// `FOR each := ...` is a FOR loop over a variable named EACH.
bool Parser::isAssignAfterEach() const {
  Tok next = ahead(2).kind;
  return next == Tok::Assign || next == Tok::Eq;
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
      {"PRIVATE", 4}, {"PUBLIC", 4}, {"PARAMETERS", 4}, {"CASE", 4}, {"OTHERWISE", 4}, {"ENDCASE", 4},
      {"SWITCH", 4}, {"ENDSWITCH", 5}, {"EACH", 4}, {"IN", 2}, {"DESCEND", 7},
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
  static const char *const standalone[] = {"EXIT", "LOOP",    "NEXT",    "END",      "ENDIF",
                                           "ENDDO", "ELSE", "ENDCASE", "OTHERWISE", "ENDSWITCH"};
  for (const char *s : standalone)
    if (std::strcmp(s, kw) == 0) return ahead().kind == Tok::EndOfStmt || ahead().kind == Tok::Eof;
  if (std::strcmp(kw, "FOR") == 0) {
    if (isKw(ahead(), "EACH") && !isAssignAfterEach()) return true;
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
  // STATICs before any other statement or function are visible in the whole file.
  while (atStmtKw("STATIC") && !atFuncStart()) {
    StmtPtr decl = parseDecl(StmtKind::Static);
    for (LocalDecl &d : static_cast<LocalStmt &>(*decl).decls) mod->fileStatics.push_back(std::move(d));
    skipBlankStmts();
  }
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

} // namespace xb
