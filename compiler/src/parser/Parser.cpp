// Token access, keyword recognition, the module and its functions, statement blocks.
#include "parser/Parser.h"

#include "common/Text.h"
#include "xbrt.h" // XB_SCOPE_*, XB_MEMBER_*, XB_OPERATOR_*

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
    // YIELD is no Clipper keyword: `yield++` increments a variable, `YIELD ++n` yields
    return std::strcmp(kw, "YIELD") == 0 && ahead(2).kind != Tok::EndOfStmt && ahead(2).kind != Tok::Eof;
  case Tok::Colon: // a message to a variable named like the keyword; in WITH OBJECT, `RETURN :name` (or `:&c`) is
                   // the keyword
    return !withObjects_.empty() && ahead().spaceBefore &&
           (ahead(2).kind == Tok::Ident ||
            (ahead(2).kind == Tok::Amp &&
             ((ahead(3).kind == Tok::Ident && !ahead(3).spaceBefore) || ahead(3).kind == Tok::LParen)));
  case Tok::Arrow: case Tok::LBracket: case Tok::Eq:
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
  if (atStmtKw("FUNCTION") || atStmtKw("PROCEDURE") || atClassStart() || atMemberStart() || atAsyncStart(0))
    return true;
  bool qualified = atStmtKw("STATIC") || (at(Tok::Ident) && (isKw(cur(), "INIT") || isKw(cur(), "EXIT")));
  return qualified &&
         (isKw(ahead(), "FUNCTION") || isKw(ahead(), "PROCEDURE") || (atStmtKw("STATIC") && atAsyncStart(1)));
}

// ASYNC FUNCTION, ASYNC PROCEDURE or ASYNC METHOD, `at` tokens ahead.
bool Parser::atAsyncStart(size_t at) const {
  const Token &t = at ? ahead(at) : cur();
  if (t.kind != Tok::Ident || !isKw(t, "ASYNC")) return false;
  const Token &n = ahead(at + 1);
  return isKw(n, "FUNCTION") || isKw(n, "PROCEDURE") || isKw(n, "METHOD");
}

// CLASS name, preceded by CREATE, STATIC, SCALAR, ABSTRACT or FINAL (two of them: STATIC ABSTRACT CLASS name), or
// INTERFACE name.
bool Parser::atClassStart() const {
  if (atStmtKw("CLASS") || atStmtKw("INTERFACE")) return ahead().kind == Tok::Ident;
  auto prefix = [&](const Token &t) {
    return isKw(t, "CREATE") || isKw(t, "STATIC") || isKw(t, "SCALAR") || isKw(t, "ABSTRACT") || isKw(t, "FINAL");
  };
  if (!atStmtKw("CREATE") && !atStmtKw("STATIC") && !atStmtKw("SCALAR") && !atStmtKw("ABSTRACT") && !atStmtKw("FINAL"))
    return false;
  size_t k = 1;
  if (prefix(ahead(k))) ++k;
  return ahead(k).kind == Tok::Ident && isKw(ahead(k), "CLASS") && ahead(k + 1).kind == Tok::Ident;
}

// METHOD, ACCESS or ASSIGN name ...: a method's implementation, outside a CLASS.
bool Parser::atMemberStart() const {
  return (atStmtKw("METHOD") || atStmtKw("ACCESS") || atStmtKw("ASSIGN") || atStmtKw("DESTRUCTOR")) &&
         ahead().kind == Tok::Ident;
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
    Request r{cur().loc, cur().upper, {}};
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
  // STATICs, MEMVARs and FIELDs before any other statement or function apply to the whole file.
  while ((atStmtKw("STATIC") && !atFuncStart()) || atExtern() || atLinkDecl() || atStmtKw("MEMVAR") ||
         atStmtKw("FIELD")) {
    if (atLinkDecl()) {
      parseLinkDecl();
    } else if (atStmtKw("FIELD")) {
      StmtPtr decl = parseDecl(StmtKind::Field);
      mod->fileFields.emplace_back(static_cast<LocalStmt *>(decl.release()));
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
    parsingFunc_ = fn.get();
    fn->body = parseBlock({});
    parsingFunc_ = nullptr;
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
    if (atClassStart()) mod->classes.push_back(parseClass());
    else mod->funcs.push_back(parseFunc());
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
  if (atAsyncStart(0)) {
    fn->async = true;
    advance(); // ASYNC
  }
  if (fn->startup == FuncDecl::Startup::None && !fn->isStatic && atMemberStart()) {
    fn->member = atKw("ACCESS") ? FuncDecl::Member::Access
                 : atKw("ASSIGN") ? FuncDecl::Member::Assign
                                  : FuncDecl::Member::Method; // METHOD, DESTRUCTOR
    fn->isProcedure = atKw("DESTRUCTOR");
    if ((isKw(ahead(), "PROCEDURE") || isKw(ahead(), "FUNCTION")) && ahead(2).kind == Tok::Ident) {
      advance(); // METHOD PROCEDURE name, METHOD FUNCTION name
      fn->isProcedure = atKw("PROCEDURE");
    }
  }
  if (fn->member == FuncDecl::Member::None) fn->isProcedure = atKw("PROCEDURE");
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
          fn->params.push_back({cur().loc, cur().upper, {}});
          advance();
          fn->params.back().declared = parseDeclaredType();
        } else {
          expect(Tok::Ident, "parameter name");
          break;
        }
      } while (accept(Tok::Comma));
    }
    expect(Tok::RParen, "')'");
  }
  if (fn->member != FuncDecl::Member::None && atKw("CLASS")) { // the class it belongs to
    advance();
    if (at(Tok::Ident)) {
      fn->memberClass = cur().upper;
      advance();
    } else {
      expect(Tok::Ident, "class name");
    }
  } else if (fn->member != FuncDecl::Member::None) {
    fn->memberClass = lastClass_;
  }
  endOfStmt();
  parsingFunc_ = fn.get();
  fn->body = parseBlock({});
  parsingFunc_ = nullptr;
  return fn;
}

// The scope a word after a member names (EXPORTED, VISIBLE, PROTECTED, HIDDEN), or, as a `label:` before members,
// also EXPORT; -1 for any other word.
static int scopeWord(const std::string &w, bool label) {
  if (w == "EXPORTED" || w == "VISIBLE" || (label && w == "EXPORT")) return XB_SCOPE_EXPORTED;
  if (w == "PROTECTED") return XB_SCOPE_PROTECTED;
  if (w == "HIDDEN") return XB_SCOPE_HIDDEN;
  return -1;
}

// [STATIC | CREATE | SCALAR | ABSTRACT | FINAL ...] CLASS name [INHERIT | FROM parent, ...] [IMPLEMENTS iface, ...],
// its members, then ENDCLASS (or END [CLASS]); INTERFACE name [INHERIT | FROM interface, ...], its METHOD, ACCESS,
// ASSIGN and VAR declarations (messages, no bodies), then ENDINTERFACE (or END [INTERFACE]):
//   EXPORTED: | EXPORT: | VISIBLE: | PROTECTED: | HIDDEN:    the scope of the members that follow
//   VAR | DATA name, ... [AS | TYPE type] [INIT | ASSIGN expr] [scope] [READONLY | RO]
//   CLASSVAR | CLASSDATA | CLASS VAR | CLASS DATA name, ... (as VAR) [SHARED]
//   FRIEND CLASS name, ... | FRIEND FUNCTION name, ...
//   DESTRUCTOR name[()]
//   METHOD name[( params )] [AS type] [CONSTRUCTOR] [ABSTRACT | FINAL] [INLINE expr, ...] [scope]
//   ACCESS name [INLINE expr, ...] [scope], ASSIGN name( param ) [INLINE expr, ...] [scope]
std::unique_ptr<ClassDecl> Parser::parseClass() {
  auto cls = std::make_unique<ClassDecl>();
  cls->loc = cur().loc;
  while (!atKw("CLASS") && !atKw("INTERFACE")) { // the prefixes
    if (atKw("STATIC")) cls->isStatic = true;
    if (atKw("SCALAR")) cls->scalar = true;
    if (atKw("ABSTRACT")) cls->isAbstract = true;
    if (atKw("FINAL")) cls->isFinal = true;
    advance();
  }
  cls->isInterface = atKw("INTERFACE");
  const char *what = cls->isInterface ? "INTERFACE" : "CLASS";
  advance();
  cls->name = cur().upper;
  if (!cls->isInterface) lastClass_ = cls->name;
  advance();
  auto nameList = [&](std::vector<std::string> &out, const char *expected) {
    advance();
    do {
      if (!at(Tok::Ident)) {
        expect(Tok::Ident, expected);
        break;
      }
      out.push_back(cur().upper);
      advance();
    } while (accept(Tok::Comma));
  };
  for (bool more = true; more;) { // INHERIT and IMPLEMENTS, in either order
    more = false;
    if (at(Tok::Ident) && (isKw(cur(), "INHERIT") || isKw(cur(), "FROM")) && cls->parents.empty()) {
      nameList(cls->parents, cls->isInterface ? "an interface" : "a parent class");
      more = true;
    }
    if (!cls->isInterface && at(Tok::Ident) && isKw(cur(), "IMPLEMENTS") && cls->interfaces.empty()) {
      nameList(cls->interfaces, "an interface");
      more = true;
    }
  }
  endOfStmt();
  uint32_t scope = XB_SCOPE_EXPORTED;
  // ABSTRACT and FINAL of a method, wherever they come among its trailing words.
  auto modifiers = [&](uint32_t &flags) {
    bool any = false;
    for (; at(Tok::Ident) && (cur().upper == "ABSTRACT" || cur().upper == "FINAL"); advance(), any = true)
      flags |= cur().upper == "ABSTRACT" ? XB_MEMBER_ABSTRACT : XB_MEMBER_FINAL;
    return any;
  };
  // Trailing scope words; READONLY for a variable, SHARED for a class variable, ABSTRACT and FINAL for a method (kept
  // with `flags`' own).
  auto memberScope = [&](uint32_t &flags, bool var, bool classVar) {
    flags = scope | (flags & (XB_MEMBER_ABSTRACT | XB_MEMBER_FINAL));
    while (at(Tok::Ident)) {
      int s = scopeWord(cur().upper, false);
      if (s >= 0) flags = (flags & ~XB_SCOPE_MASK) | (uint32_t)s;
      else if (var && (cur().upper == "READONLY" || cur().upper == "RO")) flags |= XB_MEMBER_READONLY;
      else if (classVar && cur().upper == "SHARED") flags |= XB_MEMBER_SHARED;
      else if (!var && !classVar && modifiers(flags)) continue;
      else break;
      advance();
    }
  };
  auto asType = [&] { // AS type (parseDeclaredType), or TYPE name: accepted, no hint kept for a member yet
    if (at(Tok::Ident) && cur().upper == "AS") {
      parseDeclaredType();
    } else if (at(Tok::Ident) && cur().upper == "TYPE" && ahead().kind == Tok::Ident) {
      advance();
      advance();
    }
  };
  while (true) {
    skipBlankStmts();
    if (at(Tok::Eof)) {
      diag_.error(cls->loc, std::string(what) + " " + cls->name + " is not closed by END" + what);
      break;
    }
    // END CLASS and END INTERFACE: std.ch's END rule leaves the END
    if ((!cls->isInterface && atStmtKw("ENDCLASS")) || (cls->isInterface && atStmtKw("ENDINTERFACE")) ||
        atStmtKw("END")) {
      advance();
      endOfStmt();
      break;
    }
    SourceLoc loc = cur().loc;
    if (at(Tok::Ident) && scopeWord(cur().upper, true) >= 0 && ahead().kind == Tok::Colon) { // EXPORTED:
      scope = (uint32_t)scopeWord(cur().upper, true);
      advance();
      advance();
      endOfStmt();
      continue;
    }
    bool classVar = atStmtKw("CLASSVAR") || atStmtKw("CLASSDATA") ||
                    (atStmtKw("CLASS") && (isKw(ahead(), "VAR") || isKw(ahead(), "DATA")));
    if (cls->isInterface && (atStmtKw("VAR") || atStmtKw("DATA"))) { // VAR name: the messages NAME and _NAME
      advance();
      do {
        if (!at(Tok::Ident)) {
          expect(Tok::Ident, "variable name");
          break;
        }
        for (auto kind : {FuncDecl::Member::Access, FuncDecl::Member::Assign}) {
          ClassMethodDecl m;
          m.loc = cur().loc;
          m.name = cur().upper;
          m.kind = kind;
          m.flags = XB_MEMBER_ABSTRACT;
          cls->methods.push_back(std::move(m));
        }
        advance();
      } while (accept(Tok::Comma));
      asType();
      endOfStmt();
      continue;
    }
    if (!cls->isInterface && (atStmtKw("VAR") || atStmtKw("DATA") || classVar)) {
      if (atKw("CLASS")) advance();
      advance();
      std::vector<ClassVarDecl> &vars = classVar ? cls->classVars : cls->vars;
      size_t first = vars.size();
      do {
        if (!at(Tok::Ident)) {
          expect(Tok::Ident, "variable name");
          break;
        }
        ClassVarDecl v;
        v.loc = cur().loc;
        v.name = cur().upper;
        advance();
        vars.push_back(std::move(v));
      } while (accept(Tok::Comma));
      asType();
      if (at(Tok::Ident) && (cur().upper == "INIT" || cur().upper == "ASSIGN")) { // each variable gets the expression
        advance();
        size_t at = pos_, after = pos_;
        unsigned errors = diag_.errorCount();
        for (size_t k = first; k < vars.size() && diag_.errorCount() == errors; ++k) { // errors reported once
          pos_ = at;
          vars[k].init = parseExpr();
          after = pos_;
        }
        pos_ = after;
      }
      uint32_t flags = 0;
      memberScope(flags, true, classVar);
      for (size_t k = first; k < vars.size(); ++k) vars[k].flags = flags;
      endOfStmt();
      continue;
    }
    if (cls->isInterface && !atStmtKw("METHOD") && !atStmtKw("ACCESS") && !atStmtKw("ASSIGN")) {
      diag_.error(loc, "expected a METHOD, ACCESS, ASSIGN or VAR declaration, or ENDINTERFACE, in INTERFACE " +
                           cls->name);
      skipToEndOfStmt();
      accept(Tok::EndOfStmt);
      continue;
    }
    if (atStmtKw("DESTRUCTOR") && ahead().kind == Tok::Ident) { // DESTRUCTOR name[()]
      advance();
      ClassMethodDecl m;
      m.loc = cur().loc;
      m.name = cur().upper;
      m.destructor = true;
      advance();
      if (accept(Tok::LParen)) expect(Tok::RParen, "')'");
      cls->methods.push_back(std::move(m));
      endOfStmt();
      continue;
    }
    bool async = atAsyncStart(0) && isKw(ahead(), "METHOD") && ahead(2).kind == Tok::Ident;
    if (async) advance(); // ASYNC METHOD
    bool method = atStmtKw("METHOD"), access = atStmtKw("ACCESS"), assign = atStmtKw("ASSIGN");
    if ((method || access || assign) && ahead().kind == Tok::Ident) {
      advance();
      ClassMethodDecl m;
      m.async = async;
      m.loc = cur().loc;
      m.name = cur().upper;
      m.kind = method ? FuncDecl::Member::Method : access ? FuncDecl::Member::Access : FuncDecl::Member::Assign;
      advance();
      if (accept(Tok::LParen)) {
        if (!at(Tok::RParen)) {
          do {
            if (accept(Tok::Ellipsis)) {
              m.variadic = true;
              break;
            }
            if (!at(Tok::Ident)) {
              expect(Tok::Ident, "parameter name");
              break;
            }
            m.params.push_back({cur().loc, cur().upper, {}});
            advance();
            m.params.back().declared = parseDeclaredType();
          } while (accept(Tok::Comma));
        }
        expect(Tok::RParen, "')'");
      }
      asType();
      if (method && at(Tok::Ident) && cur().upper == "OPERATOR" && ahead().kind == Tok::String) { // its message
        advance();
        m.op = operatorMessage();
      }
      if (at(Tok::Ident) && cur().upper == "CONSTRUCTOR") advance();
      modifiers(m.flags);
      if (at(Tok::Ident) && cur().upper == "INLINE") {
        if (m.async) diag_.error(cur().loc, "an INLINE method cannot be ASYNC: its expression cannot AWAIT");
        if (cls->isInterface) diag_.error(cur().loc, "an interface's method has no body: INLINE");
        parseInline(m);
      }
      memberScope(m.flags, false, false);
      if (cls->isInterface) m.flags |= XB_MEMBER_ABSTRACT;
      cls->methods.push_back(std::move(m));
      endOfStmt();
      continue;
    }
    if (atStmtKw("OPERATOR") && ahead().kind == Tok::String) { // OPERATOR "op" [ARG name, ...] INLINE expr, ...
      advance();
      ClassMethodDecl m;
      m.loc = cur().loc;
      m.op = operatorMessage();
      m.name = m.op;
      if (at(Tok::Ident) && (cur().upper == "ARG" || cur().upper == "ARGS")) {
        do {
          advance();
          if (!at(Tok::Ident)) {
            expect(Tok::Ident, "parameter name");
            break;
          }
          m.params.push_back({cur().loc, cur().upper, {}});
          advance();
          m.params.back().declared = parseDeclaredType();
        } while (at(Tok::Comma));
      }
      if (at(Tok::Ident) && cur().upper == "INLINE") parseInline(m);
      else diag_.error(cur().loc, "expected INLINE after OPERATOR, " + found());
      memberScope(m.flags, false, false);
      cls->methods.push_back(std::move(m));
      endOfStmt();
      continue;
    }
    if (atStmtKw("FRIEND") && (isKw(ahead(), "CLASS") || isKw(ahead(), "FUNCTION"))) {
      advance();
      std::vector<std::string> &names = atKw("CLASS") ? cls->friendClasses : cls->friendFunctions;
      advance();
      do {
        if (!at(Tok::Ident)) {
          expect(Tok::Ident, "a name");
          break;
        }
        names.push_back(cur().upper);
        advance();
      } while (accept(Tok::Comma));
      endOfStmt();
      continue;
    }
    diag_.error(loc, "expected a VAR, METHOD, ACCESS or ASSIGN declaration, or ENDCLASS, in CLASS " + cls->name);
    skipToEndOfStmt();
    accept(Tok::EndOfStmt);
  }
  return cls;
}

// INLINE expr, ...: the expressions evaluated in order, the last one returned.
void Parser::parseInline(ClassMethodDecl &m) {
  SourceLoc loc = cur().loc;
  advance();
  m.inlineBody = parseExpr();
  if (at(Tok::Comma)) {
    auto list = std::make_unique<ListExpr>(loc);
    list->exprs.push_back(std::move(m.inlineBody));
    while (accept(Tok::Comma)) list->exprs.push_back(parseExpr());
    m.inlineBody = std::move(list);
  }
}

// The operator a string names, as the message the runtime sends for it (XB_OPERATOR_* in xbrt.h); the
// reference's other spellings name the same operators, and "-" followed by UNARY is unary minus. An unknown one is
// reported (and kept as written).
std::string Parser::operatorMessage() {
  static const std::pair<const char *, const char *> operators[] = {
      {"+", XB_OPERATOR_ADD},  {"-", XB_OPERATOR_SUB},       {"*", XB_OPERATOR_MUL},   {"/", XB_OPERATOR_DIV},
      {"%", XB_OPERATOR_MOD},  {"^", XB_OPERATOR_POW},       {"**", XB_OPERATOR_POW},  {"=", XB_OPERATOR_EQ},
      {"==", XB_OPERATOR_EXACT_EQ}, {"!=", XB_OPERATOR_NE}, {"<>", XB_OPERATOR_NE},   {"#", XB_OPERATOR_NE},
      {"<", XB_OPERATOR_LT},   {"<=", XB_OPERATOR_LE},       {">", XB_OPERATOR_GT},    {">=", XB_OPERATOR_GE},
      {"$", XB_OPERATOR_IN},   {"!", XB_OPERATOR_NOT},       {".NOT.", XB_OPERATOR_NOT}, {"++", XB_OPERATOR_INC},
      {"--", XB_OPERATOR_DEC}, {"[]", XB_OPERATOR_INDEX}, {".AND.", XB_OPERATOR_AND}, {".OR.", XB_OPERATOR_OR},
      {"&", XB_OPERATOR_BITAND}, {"|", XB_OPERATOR_BITOR}, {"^^", XB_OPERATOR_BITXOR}, {"<<", XB_OPERATOR_SHL},
      {">>", XB_OPERATOR_SHR}, {"~", XB_OPERATOR_BITNOT}};
  std::string text = cur().text;
  SourceLoc loc = cur().loc;
  advance();
  std::string upper = upperCase(text);
  if (at(Tok::Ident) && cur().upper == "UNARY") {
    advance();
    if (upper == "-") return XB_OPERATOR_NEG;
    diag_.error(loc, "UNARY names unary minus: \"-\" UNARY");
    return text;
  }
  for (const auto &[spelling, message] : operators)
    if (upper == spelling) return message;
  diag_.error(loc, "'" + text + "' is not an operator a class can overload");
  return text;
}

// A value type: type names joined by `|` (when `alternatives`: not in a codeblock's parameters, which `|` ends), each
// named once (NUMERIC holds INTEGER), INTEGER followed by RANGE lo TO hi for the integers it holds (`ranges`: a
// RESULT's; a parameter's type has none, which no call's argument is checked against), or ANY alone (any value: no
// tags, as a parameter without AS). Clipper's and Harbour's names too: CHARACTER (STRING), CODEBLOCK (BLOCK), CLASS
// name (OBJECT), ARRAY OF type (ARRAY), USUAL and ANYTYPE (ANY), and POINTER, values xbase has none of yet (any
// value). Empty, with what is wrong in `error`, when it is no type.
std::optional<ValueType> Parser::parseValueType(bool ranges, std::string &error, bool alternatives) {
  static const std::pair<const char *, uint32_t> names[] = {
      {"NIL", 1u << XB_T_NIL},       {"LOGICAL", 1u << XB_T_LOGICAL},
      {"INTEGER", 1u << XB_T_INT},   {"DOUBLE", 1u << XB_T_DOUBLE},
      {"NUMERIC", (1u << XB_T_INT) | (1u << XB_T_DOUBLE)},
      {"STRING", 1u << XB_T_STRING}, {"CHARACTER", 1u << XB_T_STRING},
      {"DATE", 1u << XB_T_DATE},     {"ARRAY", 1u << XB_T_ARRAY},
      {"HASH", 1u << XB_T_HASH},     {"BLOCK", 1u << XB_T_BLOCK},
      {"CODEBLOCK", 1u << XB_T_BLOCK}, {"OBJECT", 1u << XB_T_OBJECT},
      {"CLASS", 1u << XB_T_OBJECT},  {"SYMBOL", 1u << XB_T_SYMBOL}};
  static const char *const anyNames[] = {"ANY", "USUAL", "ANYTYPE", "POINTER"};
  auto integer = [&](int64_t &out) {
    bool minus = accept(Tok::Minus);
    if (!at(Tok::Number) || !cur().isInt) return false;
    out = minus ? -cur().ival : cur().ival;
    advance();
    return true;
  };
  ValueType t;
  if (at(Tok::Ident) && std::find(std::begin(anyNames), std::end(anyNames), cur().upper) != std::end(anyNames)) {
    advance();
    if (alternatives && at(Tok::Pipe)) {
      error = "ANY is any value: no other type joins it";
      return std::nullopt;
    }
    return t;
  }
  do {
    uint32_t tags = 0;
    if (at(Tok::Ident))
      for (const auto &[name, bits] : names)
        if (cur().upper == name) tags = bits;
    if (!tags) {
      error = "expected a type name (NIL, LOGICAL, INTEGER, DOUBLE, NUMERIC, STRING or CHARACTER, DATE, ARRAY [OF "
              "type], HASH, BLOCK or CODEBLOCK, OBJECT or CLASS name, SYMBOL, ANY, USUAL, ANYTYPE or POINTER)";
      return std::nullopt;
    }
    std::string name = cur().upper;
    if (t.tags & tags) {
      error = "a type named twice";
      return std::nullopt;
    }
    bool isInteger = name == "INTEGER";
    t.tags |= tags;
    advance();
    if (name == "CLASS") { // CLASS name: an object of that class
      if (!at(Tok::Ident)) {
        error = "CLASS needs the class's name";
        return std::nullopt;
      }
      advance();
    } else if (name == "ARRAY" && at(Tok::Ident) && cur().upper == "OF") { // ARRAY OF type: an array
      advance();
      if (!parseValueType(false, error, false)) return std::nullopt;
    }
    if (isInteger && at(Tok::Ident) && cur().upper == "RANGE") {
      if (!ranges) {
        error = "RANGE is a RESULT's only";
        return std::nullopt;
      }
      advance();
      bool ok = integer(t.lo) && at(Tok::Ident) && cur().upper == "TO";
      if (ok) advance();
      if (!ok || !integer(t.hi) || t.lo > t.hi) {
        error = "RANGE needs two integers, lo TO hi, lo not above hi";
        return std::nullopt;
      }
      t.ranged = true;
    }
  } while (alternatives && accept(Tok::Pipe));
  return t;
}

// AS type after a declared name (a LOCAL, STATIC, PRIVATE, PUBLIC, MEMVAR, FIELD or PARAMETERS name, a parameter of a
// function, method or codeblock): Harbour's syntax, a hint for the optimizer where the variable is the function's or
// the module's (the language stays untyped: Harbour ignores it too). No tags when there is none.
ValueType Parser::parseDeclaredType(bool alternatives) {
  if (!at(Tok::Ident) || cur().upper != "AS") return {};
  SourceLoc loc = cur().loc;
  advance();
  std::string error;
  std::optional<ValueType> t = parseValueType(false, error, alternatives);
  if (!t) {
    diag_.error(loc, error + ", " + found());
    if (at(Tok::Ident)) advance(); // the name that is no type: the declaration goes on after it
    return {};
  }
  return *t;
}

// EXTERN FUNCTION name( p1 [AS type], [p2] [AS type], ... ) [NATIVE] [RESERVED] [CHECKED] [FOLD] [ARGS] [PARAMS]
// [ABBREV n] [RESULT type] [TOTAL] [CHOOSES]
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
      ValueType type;
      if (at(Tok::Ident) && cur().upper == "AS") {
        advance();
        std::string error;
        std::optional<ValueType> t = parseValueType(false, error);
        if (!t) return fail(error);
        type = *t;
      }
      d.paramTypes.push_back(type);
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
    } else if (attr == "LIBRARY") {
      advance();
      if (!at(Tok::Ident)) return fail("LIBRARY needs the library's name");
      d.library = cur().upper;
    } else if (attr == "RESULT") {
      advance();
      std::string error;
      std::optional<ValueType> t = parseValueType(true, error);
      if (!t) return fail("RESULT: " + error);
      d.result = *t;
      continue; // past the type already
    } else if (attr == "TOTAL") {
      d.total = true;
    } else if (attr == "CHOOSES") {
      d.chooses = true;
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
  if (d.native && !d.library.empty()) {
    diag_.error(d.loc, "a NATIVE function is the runtime's: it belongs to no LIBRARY");
    endOfStmt();
    return std::nullopt;
  }
  if (d.args && !d.native) {
    diag_.error(d.loc, "ARGS and PARAMS are for NATIVE functions (an xBase function cannot reach its caller's frame)");
    endOfStmt();
    return std::nullopt;
  }
  if (d.total && !d.result.tags) {
    diag_.error(d.loc, "TOTAL needs a RESULT type (a call that raises no error gives one)");
    endOfStmt();
    return std::nullopt;
  }
  if (d.chooses && (!d.native || !d.result.tags)) {
    diag_.error(d.loc, "CHOOSES is for a NATIVE function with a RESULT type (the runtime's, which its tests check)");
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
        atBlockEnd(enders) || (braceBlocks_ && at(Tok::RBrace))) // } closes an extended codeblock
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
