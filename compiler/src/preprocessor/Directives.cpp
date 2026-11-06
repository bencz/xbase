// Directives (#define, #if family, #include, rules, messages) and #if expressions.
#include "preprocessor/Preprocessor.h"

#include "preprocessor/Internal.h"
#include "preprocessor/Lexer.h"
#include "common/Text.h"
#include "xbrt.h" // shared numeric rules

#include <llvm/Support/MathExtras.h>

#include <cstdio>
#include <limits>

namespace xb {

using namespace pp;

namespace {

// Directive names may be abbreviated (see keywordMatches); returns the full
// name, or null for an unknown directive.
const char *directiveName(const std::string &up) {
  static const char *const names[] = {
      "DEFINE",  "UNDEF",     "IFDEF",      "IFNDEF",    "IF",         "ELIF",         "ELSE",
      "ENDIF",   "INCLUDE",   "COMMAND",    "XCOMMAND",  "TRANSLATE",  "XTRANSLATE",   "UNCOMMAND",
      "XUNCOMMAND", "UNTRANSLATE", "XUNTRANSLATE", "ERROR", "WARNING", "STDOUT",     "PRAGMA",
  };
  for (const char *n : names)
    if (keywordMatches(up, n)) return n;
  return nullptr;
}

// Integer evaluation of #if expressions. Overflow, division by zero, values
// that do not fit and excessive nesting make the expression invalid.
struct CondEval {
  const std::vector<Token> &t;
  size_t i = 0;
  int depth = 0;
  bool bad = false;

  const Token *cur() const { return i < t.size() ? &t[i] : nullptr; }
  bool at(Tok k) const { return cur() && cur()->kind == k; }
  int64_t fail() {
    bad = true;
    return 0;
  }

  int64_t number(const Token &k) {
    if (k.isInt) return k.ival;
    if (!xb_dbl_fits_i64(k.dval)) return fail();
    return (int64_t)k.dval;
  }

  int64_t unary() {
    if (!cur() || ++depth > kMaxConditionDepth) return fail();
    const Token &k = *cur();
    ++i;
    int64_t v = 0;
    switch (k.kind) {
    case Tok::Number: v = number(k); break;
    case Tok::True: v = 1; break;
    case Tok::False: v = 0; break;
    case Tok::Ident: v = 0; break; // undefined name
    case Tok::Not: v = !unary(); break;
    case Tok::Plus: v = unary(); break;
    case Tok::Tilde: v = xb_bit_apply(XB_BIT_NOT, unary(), 0); break;
    case Tok::Minus: {
      int64_t r = unary();
      if (llvm::SubOverflow<int64_t>(0, r, v)) v = fail();
      break;
    }
    case Tok::LParen:
      v = orExpr();
      if (at(Tok::RParen)) ++i;
      else v = fail();
      break;
    default:
      v = fail();
      break;
    }
    --depth;
    return v;
  }
  int64_t mul() {
    int64_t v = unary();
    while (!bad && (at(Tok::Star) || at(Tok::Slash) || at(Tok::Percent))) {
      Tok op = cur()->kind;
      ++i;
      int64_t r = unary();
      if (op == Tok::Star) {
        if (llvm::MulOverflow(v, r, v)) return fail();
      } else if (r == 0 || (v == std::numeric_limits<int64_t>::min() && r == -1)) {
        return fail();
      } else {
        v = op == Tok::Slash ? v / r : v % r;
      }
    }
    return v;
  }
  int64_t add() {
    int64_t v = mul();
    while (!bad && (at(Tok::Plus) || at(Tok::Minus))) {
      bool plus = at(Tok::Plus);
      ++i;
      int64_t r = mul();
      if (plus ? llvm::AddOverflow(v, r, v) : llvm::SubOverflow(v, r, v)) return fail();
    }
    return v;
  }
  int64_t shift() {
    int64_t v = add();
    while (!bad && (at(Tok::Shl) || at(Tok::Shr))) {
      xb_bit_op op = at(Tok::Shl) ? XB_BIT_SHL : XB_BIT_SAR;
      ++i;
      v = xb_bit_apply(op, v, add());
    }
    return v;
  }
  int64_t rel() {
    int64_t v = shift();
    while (!bad && cur()) {
      Tok op = cur()->kind;
      if (op != Tok::Lt && op != Tok::Le && op != Tok::Gt && op != Tok::Ge && op != Tok::Eq && op != Tok::ExactEq &&
          op != Tok::NotEq)
        break;
      ++i;
      int64_t r = shift();
      switch (op) {
      case Tok::Lt: v = v < r; break;
      case Tok::Le: v = v <= r; break;
      case Tok::Gt: v = v > r; break;
      case Tok::Ge: v = v >= r; break;
      case Tok::NotEq: v = v != r; break;
      default: v = v == r; break;
      }
    }
    return v;
  }
  // A level of the bit operators & ^^ | (C's order: below the comparisons).
  int64_t bits(int64_t (CondEval::*next)(), Tok tok, xb_bit_op op) {
    int64_t v = (this->*next)();
    while (!bad && at(tok)) {
      ++i;
      v = xb_bit_apply(op, v, (this->*next)());
    }
    return v;
  }
  int64_t bitAnd() { return bits(&CondEval::rel, Tok::Amp, XB_BIT_AND); }
  int64_t bitXor() { return bits(&CondEval::bitAnd, Tok::BitXor, XB_BIT_XOR); }
  int64_t bitOr() { return bits(&CondEval::bitXor, Tok::Pipe, XB_BIT_OR); }
  int64_t andExpr() {
    int64_t v = bitOr();
    while (!bad && at(Tok::And)) {
      ++i;
      int64_t r = bitOr();
      v = v && r;
    }
    return v;
  }
  int64_t orExpr() {
    int64_t v = andExpr();
    while (!bad && at(Tok::Or)) {
      ++i;
      int64_t r = andExpr();
      v = v || r;
    }
    return v;
  }
};

} // namespace

void Preprocessor::defineFromCommandLine(const std::string &spec) {
  size_t eq = spec.find('=');
  std::string name = spec.substr(0, eq);
  Define d;
  if (eq != std::string::npos) {
    Lexer lex(0, spec.substr(eq + 1), diag_);
    Toks t;
    lex.nextLine(t);
    d.body = std::move(t);
  }
  defines_[name] = std::move(d);
}

void Preprocessor::directive(Toks &line) {
  if (line.size() < 2) return; // a lone '#' is ignored
  const Token &nameTok = line[1];
  SourceLoc loc = line[0].loc;
  const char *full = nameTok.kind == Tok::Ident ? directiveName(nameTok.upper) : nullptr;
  std::string name = full ? full : nameTok.text;

  // Conditional compilation is tracked even inside inactive regions.
  bool isIfdef = name == "IFDEF", isIfndef = name == "IFNDEF", isIf = name == "IF";
  if (isIfdef || isIfndef || isIf) {
    bool parent = active();
    bool c = false;
    if (parent) {
      if (isIf) {
        c = evalCondition(Toks(line.begin() + 2, line.end()), loc);
      } else if (line.size() < 3 || line[2].kind != Tok::Ident) {
        diag_.error(loc, "#" + name + " requires a name");
      } else {
        bool def = defines_.count(line[2].text) != 0;
        c = isIfdef ? def : !def;
      }
    }
    conds_.push_back({parent, parent && c, c, false, loc});
    return;
  }
  if (name == "ELIF" || name == "ELSE" || name == "ENDIF") {
    if (conds_.empty()) {
      diag_.error(loc, "#" + name + " without #if");
      return;
    }
    Cond &cd = conds_.back();
    if (name == "ENDIF") {
      conds_.pop_back();
    } else if (cd.seenElse) {
      diag_.error(loc, "#" + name + " after #else");
    } else if (name == "ELSE") {
      cd.active = cd.parentActive && !cd.taken;
      cd.taken = true;
      cd.seenElse = true;
    } else { // ELIF
      if (cd.taken || !cd.parentActive) {
        cd.active = false;
      } else {
        bool c = evalCondition(Toks(line.begin() + 2, line.end()), loc);
        cd.active = c;
        cd.taken = c;
      }
    }
    return;
  }
  if (!active()) return;

  if (!full) {
    diag_.error(loc, "unknown preprocessor directive '#" + nameTok.text + "'");
    return;
  }

  if (name == "DEFINE") {
    if (line.size() < 3 || line[2].kind != Tok::Ident) {
      diag_.error(loc, "#define requires a name");
      return;
    }
    Define d;
    d.loc = line[2].loc;
    size_t i = 3;
    if (i < line.size() && line[i].kind == Tok::LParen && !line[i].spaceBefore) {
      d.funcLike = true;
      ++i;
      while (i < line.size() && line[i].kind != Tok::RParen) {
        if (line[i].kind == Tok::Ident) d.params.push_back(line[i].text);
        else if (line[i].kind != Tok::Comma) {
          diag_.error(line[i].loc, "invalid #define parameter list");
          return;
        }
        ++i;
      }
      if (i >= line.size()) {
        diag_.error(loc, "missing ')' in #define");
        return;
      }
      ++i;
    }
    d.body.assign(line.begin() + (long)i, line.end());
    if (!d.body.empty()) d.body.front().spaceBefore = false;
    auto it = defines_.find(line[2].text);
    if (it != defines_.end() && render(it->second.body) != render(d.body)) {
      diag_.warning(line[2].loc, "redefinition of '" + line[2].text + "'");
      if (it->second.loc.line) diag_.note(it->second.loc, "previous definition is here");
    }
    defines_[line[2].text] = std::move(d);
  } else if (name == "UNDEF") {
    if (line.size() < 3 || line[2].kind != Tok::Ident) diag_.error(loc, "#undef requires a name");
    else defines_.erase(line[2].text);
  } else if (name == "INCLUDE") {
    if (line.size() < 3 || line[2].kind != Tok::String) {
      diag_.error(loc, "#include expects \"filename\"");
      return;
    }
    std::string path = findInclude(line[2].text, fileStack_.empty() ? "" : fileStack_.back());
    if (path.empty()) {
      diag_.error(line[2].loc, "cannot find include file '" + line[2].text + "'");
      return;
    }
    processFile(path, line[2].loc);
  } else if (name == "COMMAND") {
    defineRule(line, true, false);
  } else if (name == "XCOMMAND") {
    defineRule(line, true, true);
  } else if (name == "TRANSLATE") {
    defineRule(line, false, false);
  } else if (name == "XTRANSLATE") {
    defineRule(line, false, true);
  } else if (name == "UNCOMMAND") {
    undefineRule(line, true, false);
  } else if (name == "XUNCOMMAND") {
    undefineRule(line, true, true);
  } else if (name == "UNTRANSLATE") {
    undefineRule(line, false, false);
  } else if (name == "XUNTRANSLATE") {
    undefineRule(line, false, true);
  } else if (name == "ERROR") {
    diag_.error(loc, "#error " + render(line, 2));
  } else if (name == "WARNING") {
    diag_.warning(loc, "#warning " + render(line, 2));
  } else if (name == "STDOUT") {
    std::printf("%s\n", render(line, 2).c_str());
  }
  // #pragma: compiler switches are not implemented yet, so it is accepted and ignored.
}

bool Preprocessor::evalCondition(Toks expr, SourceLoc loc) {
  // defined( NAME ) is resolved before macro expansion
  for (size_t i = 0; i + 3 < expr.size(); ++i) {
    if (expr[i].kind == Tok::Ident && expr[i].upper == "DEFINED" && expr[i + 1].kind == Tok::LParen &&
        expr[i + 2].kind == Tok::Ident && expr[i + 3].kind == Tok::RParen) {
      Token t;
      t.kind = defines_.count(expr[i + 2].text) ? Tok::True : Tok::False;
      t.loc = expr[i].loc;
      expr.erase(expr.begin() + (long)i, expr.begin() + (long)i + 4);
      expr.insert(expr.begin() + (long)i, t);
    }
  }
  int budget = kRewriteBudget;
  expandDefines(expr, budget);
  if (expr.empty()) {
    diag_.error(loc, "#if requires an expression");
    return false;
  }
  CondEval ev{expr};
  int64_t v = ev.orExpr();
  if (ev.bad || ev.i != expr.size()) {
    diag_.error(loc, "invalid #if expression");
    return false;
  }
  return v != 0;
}

} // namespace xb
