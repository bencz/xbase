// Token access, keyword recognition, the module and its functions, statement blocks.
#include "parser/Parser.h"

#include "common/Text.h"

#include <algorithm>
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
  if (!quiet()) diag_.error(cur().loc, std::string("expected ") + what + ", " + found());
  return false;
}

void Parser::tooDeep(const char *what, unsigned limit) {
  if (!quiet()) diag_.error(cur().loc, std::string(what) + " nested too deeply (more than " + std::to_string(limit) +
                                           " levels)");
  quietStmt_ = true;
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
    if (!quiet())
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
      {"PRIVATE", 4}, {"PUBLIC", 4}, {"PARAMETERS", 4}, {"EXTERNAL", 4}, {"CASE", 4}, {"OTHERWISE", 4}, {"ENDCASE", 4},
      {"SWITCH", 4}, {"ENDSWITCH", 5}, {"EACH", 4}, {"IN", 2}, {"DESCEND", 7},
      {"BEGIN", 4},  {"SEQUENCE", 4}, {"RECOVER", 4}, {"USING", 4}, {"INIT", 4}, {"ANNOUNCE", 4},
      {"DECLARE", 4},
  };
  if (t.kind != Tok::Ident) return false;
  size_t minLen = std::strlen(kw);
  for (const Kw &k : table)
    if (std::strcmp(k.name, kw) == 0) minLen = k.minLen;
  return keywordMatches(t.upper, kw, minLen);
}

// A keyword at statement start - unless the "keyword" is really a variable
// (`next := 1`, `exit = 2`, `end[1]`, `loop++`). Keywords that form a statement
// on their own are keywords only when nothing follows them, and FOR only when
// its statement has a TO. Before `++` or `--` the keywords that start with an
// expression (`IF --n > 0`) and the block closers stay keywords, as in Clipper:
// `next++` is an error.
bool Parser::atStmtKw(const char *kw) const {
  if (!atKw(kw)) return false;
  static const char *const keywordBeforeIncDec[] = {"IF",   "WHILE", "ELSEIF", "CASE",  "RETURN", "SWITCH",
                                                    "NEXT", "END",   "ENDIF",  "ENDDO", "ENDCASE"};
  if (isAssignment(ahead().kind)) return false;
  switch (ahead().kind) {
  case Tok::Inc: case Tok::Dec:
    for (const char *k : keywordBeforeIncDec)
      if (std::strcmp(k, kw) == 0) return true;
    return false;
  case Tok::Arrow: case Tok::Colon: case Tok::LBracket: case Tok::Eq:
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

// FUNCTION or PROCEDURE, after STATIC, INIT or EXIT.
bool Parser::atFuncStart() const {
  if (atStmtKw("FUNCTION") || atStmtKw("PROCEDURE")) return true;
  bool qualified = atStmtKw("STATIC") || (at(Tok::Ident) && (isKw(cur(), "INIT") || isKw(cur(), "EXIT")));
  return qualified && (isKw(ahead(), "FUNCTION") || isKw(ahead(), "PROCEDURE"));
}

// EXTERN[AL] FUNCTION: a declaration, at file level like FUNCTION.
bool Parser::atExtern() const { return atStmtKw("EXTERNAL") && isKw(ahead(), "FUNCTION"); }

// ANNOUNCE name, or EXTERNAL name, ...: anywhere in the module, at file level or in a function.
bool Parser::atLinkDecl() const {
  return (atStmtKw("ANNOUNCE") || (atStmtKw("EXTERNAL") && !isKw(ahead(), "FUNCTION"))) && ahead().kind == Tok::Ident;
}

void Parser::parseLinkDecl() {
  bool announces = atKw("ANNOUNCE");
  advance();
  do {
    if (!at(Tok::Ident)) {
      expect(Tok::Ident, "a function name");
      skipToEndOfStmt();
      break;
    }
    Request r{cur().loc, cur().upper};
    advance();
    if (!announces)
      requests_.push_back(std::move(r));
    else if (announce_)
      diag_.warning(r.loc, "ANNOUNCE " + r.name + " ignored: the module announces " + announce_->name + " already");
    else
      announce_ = std::move(r);
  } while (!announces && accept(Tok::Comma));
  endOfStmt();
}

// The module's EXTERNAL names, and its ANNOUNCE name: a public procedure of the module, which an EXTERNAL name in
// another module links in; an empty one (it returns NIL) unless the module defines it.
void Parser::finishModule(Module &mod) {
  mod.requests = std::move(requests_);
  if (!announce_) return;
  for (auto &f : mod.funcs)
    if (f->name == announce_->name && f->startup == FuncDecl::Startup::None) {
      if (f->isStatic)
        diag_.error(announce_->loc, "ANNOUNCE " + f->name + " names a STATIC function: it must be public");
      return;
    }
  auto fn = std::make_unique<FuncDecl>();
  fn->loc = announce_->loc;
  fn->name = announce_->name;
  fn->isProcedure = true;
  mod.funcs.push_back(std::move(fn));
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
  // STATICs and MEMVARs before any other statement or function apply to the whole file.
  while ((atStmtKw("STATIC") && !atFuncStart()) || atExtern() || atLinkDecl() || atStmtKw("MEMVAR")) {
    if (atLinkDecl()) {
      parseLinkDecl();
    } else if (atStmtKw("MEMVAR")) {
      StmtPtr decl = parseDecl(StmtKind::Memvar);
      for (LocalDecl &d : static_cast<LocalStmt &>(*decl).decls) mod->fileMemvars.push_back(std::move(d));
    } else if (atExtern()) {
      if (auto d = parseExtern()) mod->externs.push_back(std::move(*d));
    } else {
      StmtPtr decl = parseDecl(StmtKind::Static);
      for (LocalDecl &d : static_cast<LocalStmt &>(*decl).decls) mod->fileStatics.push_back(std::move(d));
    }
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
    if (atExtern()) {
      if (auto d = parseExtern()) mod->externs.push_back(std::move(*d));
      continue;
    }
    if (!atFuncStart()) { // only reachable after an error
      diag_.error(cur().loc, "statement outside of a function");
      skipToEndOfStmt();
      continue;
    }
    mod->funcs.push_back(parseFunc());
  }
  finishModule(*mod);
  return mod;
}

std::unique_ptr<FuncDecl> Parser::parseFunc() {
  auto fn = std::make_unique<FuncDecl>();
  fn->loc = cur().loc;
  if (atKw("STATIC")) {
    fn->isStatic = true;
    advance();
  } else if (atKw("INIT") || atKw("EXIT")) {
    fn->startup = atKw("INIT") ? FuncDecl::Startup::Init : FuncDecl::Startup::Exit;
    fn->isStatic = true; // the runtime runs it; nothing calls it by name
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
        if (accept(Tok::Ellipsis)) { // any number of further arguments
          fn->variadic = true;
          if (!at(Tok::RParen)) {
            diag_.error(cur().loc, "'...' must come after the last parameter");
            while (!at(Tok::RParen) && !atEndOfStmt()) advance();
          }
          break;
        }
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

// EXTERN FUNCTION name( p1, [p2], ... ) [NATIVE] [RESERVED] [CHECKED] [FOLD] [ARGS] [PARAMS] [ABBREV n]
// Required parameters come first; `...` ends the list and lifts the maximum.
// `@` before a parameter (or `...`) of a NATIVE function: it receives
// references passed to it. A malformed declaration is reported once and skipped.
std::optional<ExternDecl> Parser::parseExtern() {
  ExternDecl d;
  d.loc = cur().loc;
  auto fail = [&](const std::string &msg) -> std::optional<ExternDecl> {
    diag_.error(cur().loc, msg + ", " + found());
    skipToEndOfStmt();
    endOfStmt();
    return std::nullopt;
  };
  advance(); // EXTERN
  advance(); // FUNCTION
  if (!at(Tok::Ident)) return fail("expected the function name");
  d.name = cur().upper;
  advance();
  if (!accept(Tok::LParen)) return fail("expected '(' after the function name");
  if (!accept(Tok::RParen)) {
    while (true) {
      bool byRef = accept(Tok::At);
      if (accept(Tok::Ellipsis)) {
        d.maxArgs = ExternDecl::kUnlimited;
        d.restByRef = byRef;
        if (!accept(Tok::RParen)) return fail("expected ')' after '...'");
        break;
      }
      // [name] reaches the parser as a string ([ ] delimit strings, and listings
      // write them as "name"): a string holding a name, or @name, is an
      // optional parameter.
      bool optional = false;
      if (!byRef && at(Tok::String)) {
        std::string_view text = cur().text;
        bool atSign = !text.empty() && text.front() == '@';
        if (atSign) text.remove_prefix(1);
        optional = isIdentifier(text);
        byRef = optional && atSign;
      }
      if (!optional && !at(Tok::Ident)) return fail("expected a parameter name, [name] or '...'");
      d.byRef.push_back(byRef);
      if (!optional && d.minArgs != d.maxArgs) {
        diag_.error(cur().loc, "a required parameter cannot follow an optional one");
        skipToEndOfStmt();
        endOfStmt();
        return std::nullopt;
      }
      if (!optional) ++d.minArgs;
      ++d.maxArgs;
      advance();
      if (accept(Tok::RParen)) break;
      if (!accept(Tok::Comma)) return fail("expected ',' or ')'");
    }
  }
  while (at(Tok::Ident)) {
    const std::string &attr = cur().upper;
    if (attr == "NATIVE") d.native = true;
    else if (attr == "RESERVED") d.reserved = true;
    else if (attr == "CHECKED") d.checked = true;
    else if (attr == "FOLD") d.fold = true;
    else if (attr == "ARGS") d.args = true;
    else if (attr == "PARAMS") d.args = d.params = true;
    else if (attr == "PRIVATES") d.privates = true;
    else if (attr == "ABBREV") {
      advance();
      if (!at(Tok::Number) || !cur().isInt || cur().ival < 1 || (size_t)cur().ival > d.name.size())
        return fail("ABBREV needs a length between 1 and the length of the name");
      d.abbrev = (uint32_t)cur().ival;
    } else {
      return fail("unknown EXTERN FUNCTION attribute");
    }
    advance();
  }
  if (!atEndOfStmt()) return fail("expected an EXTERN FUNCTION attribute");
  bool anyByRef = d.restByRef || std::find(d.byRef.begin(), d.byRef.end(), true) != d.byRef.end();
  if (anyByRef && !d.native) {
    diag_.error(d.loc, "'@' parameters are for NATIVE functions (an xBase function always receives references)");
    endOfStmt();
    return std::nullopt;
  }
  if (d.args && !d.native) {
    diag_.error(d.loc, "ARGS and PARAMS are for NATIVE functions (an xBase function cannot reach its caller's frame)");
    endOfStmt();
    return std::nullopt;
  }
  endOfStmt();
  return d;
}

Block Parser::parseBlock(std::initializer_list<const char *> enders) {
  Block b;
  if (blockDepth_ == kMaxBlockNesting) {
    tooDeep("statements", kMaxBlockNesting);
    stopped_ = true;
    pos_ = toks_.size() - 1; // the end of the source
    return b;
  }
  ++blockDepth_;
  while (true) {
    skipBlankStmts();
    // EXTERN FUNCTION ends a function body, like FUNCTION; inside a nested block
    // parseStmt reports it.
    if (at(Tok::Eof) || diag_.limitReached() || atFuncStart() || (enders.size() == 0 && atExtern()) ||
        atBlockEnd(enders))
      break;
    size_t before = pos_;
    if (StmtPtr s = parseStmt()) b.push_back(std::move(s));
    if (pos_ == before) advance(); // guarantee progress after errors
  }
  --blockDepth_;
  return b;
}

void Parser::closeBlock(std::initializer_list<const char *> closers, const char *opener, SourceLoc openLoc) {
  if (atBlockEnd(closers)) {
    advance();
    endOfStmt();
    return;
  }
  if (!quiet()) diag_.error(openLoc, std::string(opener) + " without matching " + *closers.begin());
}

} // namespace xb
