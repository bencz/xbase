#include "Preprocessor.h"

#include "Lexer.h"
#include "Text.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Support/Path.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace xb {

namespace {

constexpr int kMaxIncludeDepth = 64;
constexpr int kMaxStatementDepth = 64;
// Limits shared by everything one source statement expands into, so that
// circular or self-multiplying rules stop quickly instead of exhausting memory.
constexpr int kRewriteBudget = 4096;
constexpr size_t kMaxStatementTokens = 65536;

constexpr int kMaxOptionalNesting = 64;     // [ ] nesting inside one rule
constexpr int kMaxConditionDepth = 256;     // #if expression nesting

bool isOpen(Tok k) { return k == Tok::LParen || k == Tok::LBracket || k == Tok::LBrace; }
bool isClose(Tok k) { return k == Tok::RParen || k == Tok::RBracket || k == Tok::RBrace; }
Tok closerOf(Tok k) { return k == Tok::LParen ? Tok::RParen : k == Tok::LBracket ? Tok::RBracket : Tok::RBrace; }

// Operands and closing brackets cannot be followed directly by another
// operand: that is where an expression ends (Clipper PP rule).
bool canJoin(Tok k) {
  switch (k) {
  case Tok::RParen: case Tok::RBracket: case Tok::RBrace: case Tok::Ident: case Tok::String:
  case Tok::Number: case Tok::True: case Tok::False: case Tok::Other:
    return false;
  default:
    return true;
  }
}

// Tokens that need something on their left and so cannot start an expression.
bool needLeft(const Token &t, const Token *next) {
  switch (t.kind) {
  case Tok::Assign: case Tok::PlusEq: case Tok::MinusEq: case Tok::MulEq: case Tok::DivEq:
  case Tok::ModEq: case Tok::PowEq: case Tok::ExactEq: case Tok::Eq:
    return true;
  case Tok::Colon:
    return !t.spaceBefore && next && next->kind == Tok::Ident;
  default:
    return false;
  }
}

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

} // namespace

std::string Preprocessor::findInclude(const std::string &name, const std::string &currentFile) const {
  namespace fs = llvm::sys::fs;
  namespace path = llvm::sys::path;
  if (path::is_absolute(name)) return fs::exists(name) ? name : "";
  llvm::SmallString<256> p;
  if (!currentFile.empty()) {
    p = path::parent_path(currentFile);
    path::append(p, name);
    if (fs::exists(p)) return std::string(p);
  }
  for (const std::string &d : includeDirs_) {
    p = d;
    path::append(p, name);
    if (fs::exists(p)) return std::string(p);
  }
  return "";
}

bool Preprocessor::processFile(const std::string &path, SourceLoc from) {
  if ((int)fileStack_.size() >= kMaxIncludeDepth) {
    diag_.error(from, "#include nested too deeply");
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    diag_.error(from, "cannot open '" + path + "'");
    return false;
  }
  std::stringstream ss;
  ss << in.rdbuf();

  uint32_t id = diag_.addFile(path);
  if (std::find(filesRead_.begin(), filesRead_.end(), path) == filesRead_.end()) filesRead_.push_back(path);
  Lexer lex(id, ss.str(), diag_);
  fileStack_.push_back(path);
  size_t condBase = conds_.size();

  Toks line;
  while (!diag_.limitReached() && lex.nextLine(line)) {
    if (line.empty()) continue;
    if (line[0].kind == Tok::Hash) directive(line);
    else if (active()) processLine(line);
  }

  while (conds_.size() > condBase) {
    diag_.error(conds_.back().loc, "#if/#ifdef without matching #endif");
    conds_.pop_back();
  }
  fileStack_.pop_back();
  return true;
}

std::vector<Token> Preprocessor::takeOutput() {
  Token eof;
  eof.kind = Tok::Eof;
  if (!out_.empty()) eof.loc = out_.back().loc;
  out_.push_back(eof);
  return std::move(out_);
}

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

int Preprocessor::findMarker(const Rule &r, const std::string &name) const {
  for (size_t i = 0; i < r.markers.size(); ++i)
    if (r.markers[i].name == name) return (int)i;
  return -1;
}

bool Preprocessor::parseRuleParts(Toks &line, Rule &r, bool withResult) {
  size_t arrow = line.size();
  for (size_t i = 2; i < line.size(); ++i) {
    if (line[i].kind == Tok::FatArrow && !(i > 2 && line[i - 1].kind == Tok::Backslash)) {
      arrow = i;
      break;
    }
  }
  if (withResult && arrow == line.size()) {
    diag_.error(line[0].loc, "missing '=>' in rule definition");
    return false;
  }
  if (arrow == 2) {
    diag_.error(line[0].loc, "empty rule pattern");
    return false;
  }
  int nesting = 0;
  for (size_t i = 2; i < line.size(); ++i) {
    if (line[i].kind == Tok::Backslash) {
      ++i;
    } else if (line[i].kind == Tok::LBracket && ++nesting > kMaxOptionalNesting) {
      diag_.error(line[i].loc, "optional clauses nested too deeply");
      return false;
    } else if (line[i].kind == Tok::RBracket) {
      --nesting;
    }
  }
  if (!parsePattern(line, 2, arrow, r, r.pattern, false)) return false;
  if (r.pattern.empty() || r.pattern.front().k != PNode::Lit) {
    diag_.error(line[0].loc, "rule pattern must start with a literal token");
    return false;
  }
  Toks pat(line.begin() + 2, line.begin() + (long)arrow);
  r.patternKey = upperCase(render(pat));
  if (withResult && !parseResult(line, arrow + 1, line.size(), r, r.result)) return false;
  return true;
}

void Preprocessor::defineRule(Toks &line, bool command, bool exact) {
  Rule r;
  r.command = command;
  r.exact = exact;
  if (parseRuleParts(line, r, true)) rules_.push_back(std::move(r));
}

void Preprocessor::undefineRule(Toks &line, bool command, bool exact) {
  Rule r;
  if (!parseRuleParts(line, r, false)) return;
  for (size_t i = rules_.size(); i-- > 0;) {
    if (rules_[i].command == command && rules_[i].exact == exact && rules_[i].patternKey == r.patternKey) {
      rules_.erase(rules_.begin() + (long)i);
      return;
    }
  }
}

static size_t matchingBracket(const std::vector<Token> &t, size_t open, size_t to) {
  int depth = 0;
  for (size_t i = open; i < to; ++i) {
    if (t[i].kind == Tok::Backslash) {
      ++i;
      continue;
    }
    if (t[i].kind == Tok::LBracket) ++depth;
    else if (t[i].kind == Tok::RBracket && --depth == 0) return i;
  }
  return SIZE_MAX;
}

bool Preprocessor::parseMatchMarker(const Toks &t, size_t &i, size_t to, Rule &r, bool inOpt, PNode &node) {
  auto at = [&](size_t k, Tok kind) { return i + k < to && t[i + k].kind == kind; };
  Marker m;
  size_t next;
  if (at(1, Tok::Ident) && at(2, Tok::Gt)) {
    m.kind = MK::Regular;
    m.name = t[i + 1].upper;
    next = i + 3;
  } else if (at(1, Tok::Ident) && at(2, Tok::Comma) && at(3, Tok::Ellipsis) && at(4, Tok::Gt)) {
    m.kind = MK::List;
    m.name = t[i + 1].upper;
    next = i + 5;
  } else if (at(1, Tok::Ident) && at(2, Tok::Colon)) {
    m.kind = MK::Restricted;
    m.name = t[i + 1].upper;
    size_t k = i + 3;
    Toks word;
    while (k < to && t[k].kind != Tok::Gt) {
      if (t[k].kind == Tok::Comma) {
        m.words.push_back(std::move(word));
        word.clear();
      } else {
        word.push_back(t[k]);
      }
      ++k;
    }
    if (k >= to) return false;
    // A leading or doubled comma gives an empty word (matches nothing); a trailing one does not.
    if (!word.empty() || m.words.empty()) m.words.push_back(std::move(word));
    next = k + 1;
  } else if (at(1, Tok::Star) && at(2, Tok::Ident) && at(3, Tok::Star) && at(4, Tok::Gt)) {
    m.kind = MK::Wild;
    m.name = t[i + 2].upper;
    next = i + 5;
  } else if (at(1, Tok::LParen) && at(2, Tok::Ident) && at(3, Tok::RParen) && at(4, Tok::Gt)) {
    m.kind = MK::Extended;
    m.name = t[i + 2].upper;
    next = i + 5;
  } else if (at(1, Tok::Not) && at(2, Tok::Ident) && at(3, Tok::Not) && at(4, Tok::Gt)) {
    m.kind = MK::Name;
    m.name = t[i + 2].upper;
    next = i + 5;
  } else {
    return false;
  }
  if (findMarker(r, m.name) >= 0) {
    diag_.error(t[i].loc, "duplicate match marker <" + m.name + ">");
    return false;
  }
  m.repeatable = inOpt;
  node.k = PNode::Mark;
  node.marker = (int)r.markers.size();
  node.tok = t[i];
  r.markers.push_back(std::move(m));
  i = next;
  return true;
}

bool Preprocessor::parsePattern(const Toks &t, size_t from, size_t to, Rule &r, std::vector<PNode> &out, bool inOpt) {
  size_t i = from;
  while (i < to) {
    const Token &tk = t[i];
    if (tk.kind == Tok::Backslash && i + 1 < to) {
      PNode n;
      n.tok = t[i + 1];
      out.push_back(std::move(n));
      i += 2;
      continue;
    }
    if (tk.kind == Tok::LBracket) {
      size_t close = matchingBracket(t, i, to);
      if (close == SIZE_MAX) {
        diag_.error(tk.loc, "unclosed '[' in rule pattern");
        return false;
      }
      PNode n;
      n.k = PNode::Opt;
      if (!parsePattern(t, i + 1, close, r, n.kids, true)) return false;
      if (n.kids.empty()) {
        diag_.error(tk.loc, "empty optional clause in rule pattern");
        return false;
      }
      out.push_back(std::move(n));
      i = close + 1;
      continue;
    }
    if (tk.kind == Tok::RBracket) {
      diag_.error(tk.loc, "unbalanced ']' in rule pattern");
      return false;
    }
    if (tk.kind == Tok::Lt) {
      PNode n;
      unsigned errs = diag_.errorCount();
      if (parseMatchMarker(t, i, to, r, inOpt, n)) {
        out.push_back(std::move(n));
        continue;
      }
      if (diag_.errorCount() != errs) return false;
    }
    PNode n;
    n.tok = tk;
    out.push_back(std::move(n));
    ++i;
  }
  return true;
}

bool Preprocessor::parseResult(const Toks &t, size_t from, size_t to, Rule &r, std::vector<RNode> &out) {
  size_t i = from;
  auto at = [&](size_t k, Tok kind) { return i + k < to && t[i + k].kind == kind; };
  auto marker = [&](const Token &tk) { return tk.kind == Tok::Ident ? findMarker(r, tk.upper) : -1; };
  auto push = [&](int m, RK rk, size_t len) {
    RNode n;
    n.k = RNode::Mark;
    n.marker = m;
    n.rk = rk;
    n.tok = t[i];
    out.push_back(std::move(n));
    i += len;
  };

  while (i < to) {
    const Token &tk = t[i];
    if (tk.kind == Tok::Backslash && i + 1 < to) {
      RNode n;
      n.tok = t[i + 1];
      n.tok.spaceBefore = tk.spaceBefore;
      out.push_back(std::move(n));
      i += 2;
      continue;
    }
    if (tk.kind == Tok::LBracket) {
      size_t close = matchingBracket(t, i, to);
      if (close == SIZE_MAX) {
        diag_.error(tk.loc, "unclosed '[' in rule result");
        return false;
      }
      RNode n;
      n.k = RNode::Opt;
      n.tok = tk;
      if (!parseResult(t, i + 1, close, r, n.kids)) return false;
      out.push_back(std::move(n));
      i = close + 1;
      continue;
    }
    int m;
    if (tk.kind == Tok::Hash && at(1, Tok::Lt) && at(3, Tok::Gt) && (m = marker(t[i + 2])) >= 0) {
      push(m, RK::Dumb, 4);
      continue;
    }
    if (tk.kind == Tok::Lt) {
      if (at(2, Tok::Gt) && (m = marker(t[i + 1])) >= 0) {
        push(m, RK::Regular, 3);
        continue;
      }
      if (at(1, Tok::Ident) && at(2, Tok::Comma) && at(3, Tok::Ellipsis) && at(4, Tok::Gt) &&
          (m = marker(t[i + 1])) >= 0) {
        push(m, RK::Regular, 5);
        continue;
      }
      if (at(1, Tok::String) && at(2, Tok::Gt) && (m = findMarker(r, upperCase(t[i + 1].text))) >= 0) {
        push(m, RK::Normal, 3);
        continue;
      }
      if (at(1, Tok::LParen) && at(3, Tok::RParen) && at(4, Tok::Gt) && (m = marker(t[i + 2])) >= 0) {
        push(m, RK::Smart, 5);
        continue;
      }
      if (at(1, Tok::LBrace) && at(3, Tok::RBrace) && at(4, Tok::Gt) && (m = marker(t[i + 2])) >= 0) {
        push(m, RK::Blockify, 5);
        continue;
      }
      if (at(1, Tok::Dot) && at(3, Tok::Dot) && at(4, Tok::Gt) && (m = marker(t[i + 2])) >= 0) {
        push(m, RK::Logify, 5);
        continue;
      }
      // <.T.> etc.: the tokenizer read ".x." as a logical/operator word
      if (i + 2 < to && t[i + 2].kind == Tok::Gt && t[i + 1].text.size() > 2 && t[i + 1].text.front() == '.' &&
          t[i + 1].text.back() == '.') {
        std::string inner = upperCase(t[i + 1].text.substr(1, t[i + 1].text.size() - 2));
        if ((m = findMarker(r, inner)) >= 0) {
          push(m, RK::Logify, 3);
          continue;
        }
      }
      if (at(1, Tok::Minus) && at(3, Tok::Minus) && at(4, Tok::Gt) && marker(t[i + 2]) >= 0) {
        i += 5; // <-x->: matched but produces nothing
        continue;
      }
    }
    RNode n;
    n.tok = tk;
    out.push_back(std::move(n));
    ++i;
  }
  return true;
}

bool Preprocessor::tokEqual(const Token &a, const Token &p, Cmp mode) {
  if (a.kind != p.kind) return false;
  switch (a.kind) {
  case Tok::Ident:
    if (mode == Cmp::Case) return a.text == p.text;
    if (mode == Cmp::Std) return a.upper == p.upper;
    return keywordMatches(a.upper, p.upper);
  case Tok::String:
  case Tok::Number:
  case Tok::Other:
    return a.text == p.text;
  case Tok::NotEq: // != and <> are one operator; # is a different token
    return (a.text == "#") == (p.text == "#");
  default:
    return true; // same operator/punctuation kind
  }
}

bool Preprocessor::canStartExp(const Toks &in, size_t pos, size_t end) {
  if (pos >= end) return false;
  const Token *next = pos + 1 < end ? &in[pos + 1] : nullptr;
  if (needLeft(in[pos], next)) return false;
  if (in[pos].kind == Tok::LBracket) {
    for (size_t i = pos + 1; i < end; ++i)
      if (in[i].kind == Tok::RBracket) return true;
    return false;
  }
  return true;
}

bool Preprocessor::skipExp(const Toks &in, size_t &pos, size_t end, const PNode *stop, Cmp mode, bool *fStop) {
  size_t start = pos;
  Tok prev = Tok::Eof; // "nothing yet": can be joined
  Tok lbr = Tok::Eof, rbr = Tok::Eof;
  int depth = 0;
  if (fStop) *fStop = false;
  for (;;) {
    if (pos >= end) {
      if (fStop) *fStop = true;
      break;
    }
    const Token &t = in[pos];
    if (depth) {
      if (t.kind == lbr) ++depth;
      else if (t.kind == rbr) --depth;
    } else if (t.kind == Tok::Comma) {
      if (fStop) ++pos; // list: the comma belongs to the list
      break;
    } else if (isClose(t.kind) || (!canJoin(t.kind) && !canJoin(prev)) ||
               (stop && stop->k == PNode::Lit && tokEqual(t, stop->tok, mode))) {
      if (fStop) *fStop = true;
      break;
    } else if (isOpen(t.kind)) {
      lbr = t.kind;
      rbr = closerOf(t.kind);
      depth = 1;
    }
    if (t.kind != Tok::Inc && t.kind != Tok::Dec) prev = t.kind;
    ++pos;
  }
  return pos != start;
}

// &name, &name. or &( expr )
bool Preprocessor::skipMacro(const Toks &in, size_t &pos, size_t end) {
  if (pos + 1 >= end || in[pos].kind != Tok::Amp) return false;
  if (in[pos + 1].kind == Tok::Ident) {
    pos += 2;
    if (pos < end && in[pos].kind == Tok::Dot && !in[pos].spaceBefore) ++pos;
    return true;
  }
  if (in[pos + 1].kind == Tok::LParen) {
    int depth = 0;
    for (size_t i = pos + 1; i < end; ++i) {
      if (in[i].kind == Tok::LParen) ++depth;
      else if (in[i].kind == Tok::RParen && --depth == 0) {
        pos = i + 1;
        return true;
      }
    }
  }
  return false;
}

bool Preprocessor::matchMarker(const Marker &m, const Toks &in, size_t &pos, size_t end, const PNode *stop,
                               Cmp mode) const {
  switch (m.kind) {
  case MK::Regular:
    return canStartExp(in, pos, end) && skipExp(in, pos, end, stop, mode, nullptr);
  case MK::List: {
    if (!canStartExp(in, pos, end)) return false;
    bool matched = false, fStop = false;
    do {
      if (!skipExp(in, pos, end, stop, mode, &fStop)) break;
      matched = true;
    } while (!fStop);
    return matched;
  }
  case MK::Restricted:
    for (const Toks &w : m.words) {
      if (w.empty()) return true; // <x: ,...> always matches nothing
      if (w.size() == 1 && w[0].kind == Tok::Amp) {
        if (skipMacro(in, pos, end)) return true;
        continue;
      }
      size_t p = pos, k = 0;
      while (k < w.size() && p < end && tokEqual(in[p], w[k], mode)) {
        ++p;
        ++k;
      }
      if (k == w.size()) {
        pos = p;
        return true;
      }
    }
    return false;
  case MK::Wild:
    if (pos >= end) return false;
    pos = end;
    return true;
  case MK::Extended:
    if (pos >= end || in[pos].kind == Tok::RParen || in[pos].kind == Tok::RBracket || in[pos].kind == Tok::Comma ||
        !canStartExp(in, pos, end))
      return false;
    if (in[pos].kind == Tok::LParen) return skipExp(in, pos, end, stop, mode, nullptr);
    do {
      ++pos;
    } while (pos < end && !in[pos].spaceBefore && in[pos].kind != Tok::Comma);
    return true;
  case MK::Name:
    if (pos < end && in[pos].kind == Tok::Ident) {
      ++pos;
      return true;
    }
    return false;
  }
  return false;
}

// Mirrors Clipper's pattern matcher: optional clauses that follow each other
// may match in any order and any number of times; a marker that is not inside
// an optional clause may match only once.
bool Preprocessor::matchSeq(const Rule &r, const std::vector<PNode> &nodes, const Toks &in, size_t &pos, size_t end,
                            const PNode *outerStop, Caps &caps) const {
  Cmp mode = r.exact ? Cmp::Std : Cmp::DBase;
  size_t k = 0, n = nodes.size();
  while (k < n && pos < end) {
    const PNode &node = nodes[k];
    if (node.k == PNode::Opt) {
      size_t j = k;
      while (j < n && nodes[j].k == PNode::Opt) ++j;
      const PNode *newStop = j < n ? &nodes[j] : outerStop;
      size_t o = k;
      std::vector<size_t> saved(caps.size());
      while (o < j && pos < end) {
        size_t p = pos;
        for (size_t c = 0; c < caps.size(); ++c) saved[c] = caps[c].size();
        if (matchSeq(r, nodes[o].kids, in, p, end, newStop, caps) && p != pos) {
          pos = p;
          o = k; // restart: clauses may come in any order
        } else {
          for (size_t c = 0; c < caps.size(); ++c) caps[c].resize(saved[c]); // captures only ever grow
          ++o;
        }
      }
      k = j;
      continue;
    }
    if (node.k == PNode::Lit) {
      if (!tokEqual(in[pos], node.tok, mode)) return false;
      ++pos;
    } else {
      const PNode *stop = outerStop ? outerStop : (k + 1 < n ? &nodes[k + 1] : nullptr);
      size_t before = pos;
      const Marker &m = r.markers[(size_t)node.marker];
      if (!matchMarker(m, in, pos, end, stop, mode)) return false;
      if (pos != before) {
        auto &occ = caps[(size_t)node.marker];
        if (!occ.empty() && !m.repeatable) return false;
        occ.emplace_back(in.begin() + (long)before, in.begin() + (long)pos);
      }
    }
    ++k;
  }
  while (k < n && nodes[k].k == PNode::Opt) ++k;
  return k == n;
}

Token Preprocessor::makeString(const std::string &value, SourceLoc loc, bool space) {
  Token t;
  t.kind = Tok::String;
  t.loc = loc;
  t.spaceBefore = space;
  t.text = value;
  return t;
}

std::vector<Preprocessor::Toks> Preprocessor::splitList(const Toks &t) {
  std::vector<Toks> items(1);
  int depth = 0;
  for (const Token &tk : t) {
    if (isOpen(tk.kind)) ++depth;
    else if (isClose(tk.kind)) --depth;
    if (tk.kind == Tok::Comma && depth == 0) {
      items.emplace_back();
      continue;
    }
    items.back().push_back(tk);
  }
  return items;
}

// An optional result clause repeats once per match of the markers it contains
// directly (markers of nested clauses do not count).
size_t Preprocessor::repeatCount(const std::vector<RNode> &nodes, const Caps &caps) {
  size_t n = 0;
  for (const RNode &nd : nodes)
    if (nd.k == RNode::Mark) n = std::max(n, caps[(size_t)nd.marker].size());
  return n;
}

void Preprocessor::buildResult(const Rule &r, const std::vector<RNode> &nodes, const Caps &caps, int rep,
                               SourceLoc loc, Toks &out) const {
  for (const RNode &nd : nodes) {
    if (nd.k == RNode::Lit) {
      Token t = nd.tok;
      t.loc = loc;
      out.push_back(std::move(t));
      continue;
    }
    if (nd.k == RNode::Opt) {
      size_t times = repeatCount(nd.kids, caps);
      for (size_t i = 0; i < times; ++i) buildResult(r, nd.kids, caps, (int)i, loc, out);
      continue;
    }

    // A marker that matched once supplies that value to every repetition.
    const auto &occs = caps[(size_t)nd.marker];
    const Toks *val = nullptr;
    size_t idx = rep >= 0 ? (size_t)rep : 0;
    if (occs.size() == 1) val = &occs[0];
    else if (idx < occs.size()) val = &occs[idx];
    bool space = nd.tok.spaceBefore;
    auto add = [&](Tok k, const char *s, bool spc) {
      Token t;
      t.kind = k;
      t.text = s;
      t.loc = loc;
      t.spaceBefore = spc;
      out.push_back(t);
    };
    auto clone = [&](Toks::const_iterator from, Toks::const_iterator to, bool spc) {
      size_t first = out.size();
      out.insert(out.end(), from, to);
      if (out.size() > first) out[first].spaceBefore = spc;
    };

    switch (nd.rk) {
    case RK::Regular:
      if (val) clone(val->begin(), val->end(), space);
      break;
    case RK::Dumb:
      out.push_back(makeString(val ? render(*val) : "", loc, space));
      break;
    case RK::Logify:
      add(val ? Tok::True : Tok::False, val ? ".T." : ".F.", space);
      break;
    case RK::Normal:
    case RK::Smart:
    case RK::Blockify: {
      if (!val) break;
      // Every comma-separated element is converted on its own.
      std::vector<Toks> items = splitList(*val);
      for (size_t i = 0; i < items.size(); ++i) {
        if (i) add(Tok::Comma, ",", false);
        const Toks &it = items[i];
        if (it.empty()) continue;
        bool sp = i == 0 ? space : it[0].spaceBefore;
        bool isBlock = it.size() >= 2 && it[0].kind == Tok::LBrace && it[1].kind == Tok::Pipe;
        bool isMacroVar = it.size() >= 2 && it.size() <= 3 && it[0].kind == Tok::Amp && it[1].kind == Tok::Ident &&
                          (it.size() == 2 || (it[2].kind == Tok::Dot && !it[2].spaceBefore));
        bool isMacroExpr = it.size() >= 2 && it[0].kind == Tok::Amp && it[1].kind == Tok::LParen;
        if (nd.rk == RK::Blockify) {
          if (isBlock) {
            clone(it.begin(), it.end(), sp);
          } else {
            add(Tok::LBrace, "{", sp);
            add(Tok::Pipe, "|", false);
            add(Tok::Pipe, "|", false);
            clone(it.begin(), it.end(), true);
            add(Tok::RBrace, "}", true);
          }
        } else if (isMacroVar) {
          clone(it.begin() + 1, it.begin() + 2, sp); // &var -> var
        } else if (isMacroExpr) {
          clone(it.begin() + 1, it.end(), sp); // &( expr ) -> ( expr )
        } else if (nd.rk == RK::Smart && (it[0].kind == Tok::String || it[0].kind == Tok::LParen)) {
          clone(it.begin(), it.end(), sp);
        } else {
          out.push_back(makeString(render(it), loc, sp));
        }
      }
      break;
    }
    }
  }
}

void Preprocessor::processLine(Toks &line) {
  Toks st;
  auto flush = [&] {
    if (st.empty()) return;
    Rewrite rw{kRewriteBudget}; // one set of limits per source statement
    processStatement(std::move(st), 0, rw);
    st.clear();
  };
  for (Token &t : line) {
    if (t.kind == Tok::Semicolon) flush();
    else st.push_back(std::move(t));
  }
  flush();
}

bool Preprocessor::splitStatements(Toks &st, int depth, Rewrite &rw) {
  if (std::none_of(st.begin(), st.end(), [](const Token &t) { return t.kind == Tok::Semicolon; })) return false;
  Toks part;
  for (Token &t : st) {
    if (t.kind == Tok::Semicolon) {
      if (!part.empty()) processStatement(std::move(part), depth + 1, rw);
      part.clear();
    } else {
      part.push_back(std::move(t));
    }
  }
  if (!part.empty()) processStatement(std::move(part), depth + 1, rw);
  return true;
}

void Preprocessor::rewriteFailed(Rewrite &rw, SourceLoc loc, const char *msg) {
  if (!rw.failed) diag_.error(loc, msg);
  rw.failed = true;
}

void Preprocessor::processStatement(Toks st, int depth, Rewrite &rw) {
  if (st.empty() || rw.failed) return;
  SourceLoc loc = st[0].loc;
  if (depth > kMaxStatementDepth) {
    rewriteFailed(rw, loc, "preprocessor rules nested too deeply (circular rules?)");
    return;
  }
  for (;;) {
    bool changed = expandDefines(st, rw.budget);
    if (splitStatements(st, depth, rw)) return;
    while (rw.budget > 0 && st.size() <= kMaxStatementTokens && applyTranslates(st, rw.budget)) {
      changed = true;
      if (splitStatements(st, depth, rw)) return;
    }
    if (st.empty()) return; // expanded to nothing
    if (rw.budget > 0 && st.size() <= kMaxStatementTokens && applyCommand(st, rw.budget)) {
      changed = true;
      if (splitStatements(st, depth, rw)) return;
    }
    if (rw.budget <= 0 || st.size() > kMaxStatementTokens) {
      rewriteFailed(rw, loc, "circular #define or preprocessor rule");
      return;
    }
    if (!changed || st.empty()) break;
  }
  emit(st);
}

void Preprocessor::emit(const Toks &st) {
  if (st.empty()) return;
  out_.insert(out_.end(), st.begin(), st.end());
  Token eos;
  eos.kind = Tok::EndOfStmt;
  eos.loc = st.back().loc;
  out_.push_back(eos);
}

bool Preprocessor::expandDefines(Toks &st, int &budget) {
  bool changed = false;
  for (size_t i = 0; i < st.size() && budget > 0 && st.size() <= kMaxStatementTokens;) {
    if (st[i].kind != Tok::Ident) {
      ++i;
      continue;
    }
    auto it = defines_.find(st[i].text); // #define is case-sensitive
    if (it == defines_.end()) {
      ++i;
      continue;
    }
    const Define &d = it->second;
    SourceLoc loc = st[i].loc;
    bool space = st[i].spaceBefore;
    Toks repl;
    size_t endPos;
    if (!d.funcLike) {
      repl = d.body;
      endPos = i + 1;
    } else {
      if (i + 1 >= st.size() || st[i + 1].kind != Tok::LParen) {
        ++i;
        continue;
      }
      std::vector<Toks> args(1);
      int depthP = 0;
      size_t k = i + 1;
      for (; k < st.size(); ++k) {
        Tok kk = st[k].kind;
        if (isOpen(kk)) {
          if (depthP++ == 0) continue;
        } else if (isClose(kk)) {
          if (--depthP == 0) break;
        } else if (kk == Tok::Comma && depthP == 1) {
          args.emplace_back();
          continue;
        }
        args.back().push_back(st[k]);
      }
      if (k >= st.size()) {
        ++i;
        continue;
      }
      if (args.size() == 1 && args[0].empty() && d.params.empty()) args.clear();
      if (args.size() != d.params.size()) { // not a call of this macro: left as written
        ++i;
        continue;
      }
      for (const Token &b : d.body) {
        auto p = std::find(d.params.begin(), d.params.end(), b.text);
        if (b.kind == Tok::Ident && p != d.params.end()) {
          const Toks &a = args[(size_t)(p - d.params.begin())];
          size_t first = repl.size();
          repl.insert(repl.end(), a.begin(), a.end());
          if (repl.size() > first) repl[first].spaceBefore = b.spaceBefore;
        } else {
          repl.push_back(b);
        }
      }
      endPos = k + 1;
    }
    for (Token &t : repl) t.loc = loc;
    if (!repl.empty()) repl[0].spaceBefore = space;
    st.erase(st.begin() + (long)i, st.begin() + (long)endPos);
    st.insert(st.begin() + (long)i, repl.begin(), repl.end());
    changed = true;
    --budget; // rescan the expansion: nested defines expand too
  }
  return changed;
}

bool Preprocessor::applyTranslates(Toks &st, int &budget) {
  for (size_t i = 0; i < st.size(); ++i) {
    for (size_t ri = rules_.size(); ri-- > 0;) {
      const Rule &r = rules_[ri];
      if (r.command) continue;
      if (!tokEqual(st[i], r.pattern[0].tok, r.exact ? Cmp::Std : Cmp::DBase)) continue;
      Caps caps(r.markers.size());
      size_t pos = i;
      if (!matchSeq(r, r.pattern, st, pos, st.size(), nullptr, caps) || pos == i) continue;
      Toks res;
      buildResult(r, r.result, caps, -1, st[i].loc, res);
      if (!res.empty()) res[0].spaceBefore = st[i].spaceBefore;
      if (render(res) == render(st, i, pos)) continue; // identity rewrite
      st.erase(st.begin() + (long)i, st.begin() + (long)pos);
      st.insert(st.begin() + (long)i, res.begin(), res.end());
      --budget;
      return true;
    }
  }
  return false;
}

bool Preprocessor::applyCommand(Toks &st, int &budget) {
  for (size_t ri = rules_.size(); ri-- > 0;) {
    const Rule &r = rules_[ri];
    if (!r.command) continue;
    if (!tokEqual(st[0], r.pattern[0].tok, r.exact ? Cmp::Std : Cmp::DBase)) continue;
    Caps caps(r.markers.size());
    size_t pos = 0;
    if (!matchSeq(r, r.pattern, st, pos, st.size(), nullptr, caps) || pos != st.size()) continue;
    Toks res;
    buildResult(r, r.result, caps, -1, st[0].loc, res);
    if (!res.empty()) res[0].spaceBefore = false;
    if (render(res) == render(st)) return false; // identity rewrite
    st = std::move(res);
    --budget;
    return true;
  }
  return false;
}

namespace {
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
    constexpr double kLimit = 9223372036854775808.0; // 2^63
    if (!std::isfinite(k.dval) || k.dval >= kLimit || k.dval < -kLimit) return fail();
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
  int64_t rel() {
    int64_t v = add();
    while (!bad && cur()) {
      Tok op = cur()->kind;
      if (op != Tok::Lt && op != Tok::Le && op != Tok::Gt && op != Tok::Ge && op != Tok::Eq && op != Tok::ExactEq &&
          op != Tok::NotEq)
        break;
      ++i;
      int64_t r = add();
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
  int64_t andExpr() {
    int64_t v = rel();
    while (!bad && at(Tok::And)) {
      ++i;
      int64_t r = rel();
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

// Writes a string literal that reads back as `value`. Like Clipper, the
// delimiter is chosen from the value, not kept from the source: "..." first,
// then '...', then [...], and a concatenation when the value contains all three.
// Stringification uses this, so it decides the text a string token produces.
std::string Preprocessor::quoteString(const std::string &value) {
  auto fits = [&](char open) { return value.find(open == '[' ? ']' : open) == std::string::npos; };
  for (char open : {'"', '\'', '['}) {
    if (fits(open)) return std::string(1, open) + value + (open == '[' ? ']' : open);
  }
  std::string s;
  size_t start = 0;
  for (size_t q = value.find('"'); q != std::string::npos; q = value.find('"', start)) {
    s += '"' + value.substr(start, q - start) + R"(" + '"' + )";
    start = q + 1;
  }
  return s + '"' + value.substr(start) + '"';
}

std::string Preprocessor::render(const std::vector<Token> &toks, size_t from, size_t to) {
  std::string s;
  to = std::min(to, toks.size());
  for (size_t i = from; i < to; ++i) {
    const Token &t = toks[i];
    if (i > from && t.spaceBefore && toks[i - 1].kind != Tok::EndOfStmt) s += ' ';
    if (t.kind == Tok::String) {
      s += quoteString(t.text);
    } else if (t.kind == Tok::EndOfStmt) {
      s += '\n';
    } else {
      s += t.text;
    }
  }
  return s;
}

} // namespace xb
