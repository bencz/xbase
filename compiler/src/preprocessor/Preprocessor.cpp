// The driver: source files and includes, statement splitting and rewriting by #define,
// #translate and #command, and the text of the output.
#include "preprocessor/Preprocessor.h"

#include "preprocessor/Internal.h"
#include "preprocessor/Lexer.h"

#include "common/Text.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace xb {

using namespace pp;

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
  if (std::find(filesRead_.begin(), filesRead_.end(), path) == filesRead_.end()) filesRead_.push_back(path);
  processText(path, ss.str());
  return true;
}

void Preprocessor::processText(const std::string &name, const std::string &text) {
  uint32_t id = diag_.addFile(name);
  Lexer lex(id, text, diag_);
  Lexer *outer = std::exchange(lexer_, &lex);
  fileStack_.push_back(name);
  size_t condBase = conds_.size();

  Toks line;
  while (!diag_.limitReached() && lex.nextLine(line)) {
    if (line.empty()) continue;
    if (line[0].kind == Tok::Hash) {
      directive(line);
    } else if (active()) {
      joinBlocks(lex, line);
      processLine(line);
    }
  }

  while (conds_.size() > condBase) {
    diag_.error(conds_.back().loc, "#if/#ifdef without matching #endif");
    conds_.pop_back();
  }
  fileStack_.pop_back();
  lexer_ = outer;
}

std::vector<Token> Preprocessor::takeOutput() {
  Token eof;
  eof.kind = Tok::Eof;
  if (!out_.empty()) eof.loc = out_.back().loc;
  out_.push_back(eof);
  return std::move(out_);
}

// Whether the line ends opening an extended codeblock: {| params | and nothing after it.
static bool opensBlock(const std::vector<Token> &line) {
  size_t n = line.size();
  if (n < 3 || line[n - 1].kind != Tok::Pipe) return false;
  size_t k = n - 2;
  bool wantName = true; // going left: a name, then a comma, ...
  while (k > 0 && line[k].kind != Tok::Pipe) {
    if (line[k].kind != (wantName ? Tok::Ident : Tok::Comma)) return false;
    wantName = !wantName;
    --k;
  }
  return k > 0 && line[k].kind == Tok::Pipe && line[k - 1].kind == Tok::LBrace && (!wantName || k == n - 2);
}

// A line that opens an extended codeblock takes the lines up to the one that starts with its }: they are
// preprocessed as statements of their own, and their output goes into the line as one token (Tok::BlockBody), so a
// rule sees the codeblock whole, as one operand; emit puts the statements back. The line that closes it goes on the
// line, and may open another one.
void Preprocessor::joinBlocks(Lexer &lex, Toks &line) {
  while (opensBlock(line)) {
    SourceLoc open = line.back().loc;
    Toks body, next;
    std::swap(out_, body); // what the statements emit goes to `body`
    bool closed = false;
    while (!diag_.limitReached() && lex.nextLine(next)) {
      if (next.empty()) continue;
      if (next[0].kind == Tok::Hash) {
        directive(next);
        continue;
      }
      if (!active()) continue; // a skipped #if part: its } lines close nothing
      if (next[0].kind == Tok::RBrace) {
        closed = true;
        break;
      }
      joinBlocks(lex, next);
      processLine(next);
    }
    std::swap(out_, body);
    if (!closed) {
      diag_.error(open, "codeblock without a line starting with '}'");
      return;
    }
    Token t;
    t.kind = Tok::BlockBody;
    t.loc = open;
    t.ival = (int64_t)blockBodies_.size();
    blockBodies_.push_back(std::move(body));
    line.push_back(std::move(t));
    line.insert(line.end(), next.begin(), next.end());
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
  if (!pendingText_.empty()) textBlock();
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
  if (isTextForm(st)) pendingText_ = std::move(st);
  else emit(st);
}

// `text <lineFunc> [, <endFunc>]`: the result of std.ch's TEXT (Clipper's form).
bool Preprocessor::isTextForm(const Toks &st) {
  return (st.size() == 2 || (st.size() == 4 && st[2].kind == Tok::Comma && st[3].kind == Tok::Ident)) &&
         st[0].kind == Tok::Ident && st[0].upper == "TEXT" && st[1].kind == Tok::Ident;
}

// The lines after the one with the `text` statement (whose other statements come first), as written, up to the one
// that ends them: ENDTEXT as a whole word in any letter case, then nothing but blanks, a comment or a `;` and
// statements, which are compiled. Each line becomes `<lineFunc>( "line" )`, a string literal (so `&name` in it is
// substituted when the code runs), and the end `<endFunc>()`; these are rewritten like any statement.
void Preprocessor::textBlock() {
  Toks st = std::move(pendingText_);
  pendingText_.clear();
  auto call = [&](const Token &fn, const Token *arg, SourceLoc loc) {
    Toks c{fn};
    c[0].loc = loc;
    Token t;
    t.kind = Tok::LParen;
    t.text = "(";
    t.loc = loc;
    c.push_back(t);
    if (arg) c.push_back(*arg);
    t.kind = Tok::RParen;
    t.text = ")";
    c.push_back(t);
    Rewrite rw{kRewriteBudget};
    processStatement(std::move(c), 0, rw);
  };
  auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\f' || c == '\x1a'; };
  auto nameChar = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
  std::string text;
  SourceLoc loc;
  while (lexer_ && lexer_->rawLine(text, loc)) {
    size_t from = 0;
    while (from < text.size() && blank(text[from])) ++from;
    size_t to = from;
    while (to < text.size() && nameChar(text[to])) ++to;
    size_t rest = to;
    while (rest < text.size() && blank(text[rest])) ++rest;
    std::string_view after = std::string_view(text).substr(rest);
    bool ends = upperCase(std::string_view(text).substr(from, to - from)) == "ENDTEXT" &&
                (after.empty() || after[0] == ';' || after.substr(0, 2) == "//" || after.substr(0, 2) == "&&" ||
                 after.substr(0, 2) == "/*");
    if (!ends) {
      Token line;
      line.kind = Tok::String;
      line.text = text;
      line.loc = loc;
      call(st[1], &line, loc);
      continue;
    }
    if (st.size() == 4) call(st[3], nullptr, loc);
    if (!after.empty() && after[0] == ';') { // the statements after it, on the rest of the line
      SourceLoc at{loc.file, loc.line, loc.col + (uint32_t)rest + 1};
      Lexer part(loc.file, std::string(after.substr(1)), diag_, at);
      Toks line;
      while (part.nextLine(line)) {
        joinBlocks(*lexer_, line);
        processLine(line);
      }
    }
    return;
  }
  diag_.error(st[0].loc, "TEXT without ENDTEXT");
}

void Preprocessor::emit(const Toks &st) {
  if (st.empty()) return;
  Token eos;
  eos.kind = Tok::EndOfStmt;
  for (const Token &t : st) {
    if (t.kind != Tok::BlockBody) {
      out_.push_back(t);
      continue;
    }
    eos.loc = t.loc; // the line that opens the codeblock ends, then its statements (each with its end)
    out_.push_back(eos);
    const Toks &body = blockBodies_.at((size_t)t.ival);
    out_.insert(out_.end(), body.begin(), body.end());
  }
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
      overflow_ = false;
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
    overflow_ = false;
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

std::string Preprocessor::render(const std::vector<Token> &toks, size_t from, size_t to) const {
  std::string s;
  to = std::min(to, toks.size());
  for (size_t i = from; i < to; ++i) {
    const Token &t = toks[i];
    if (i > from && t.spaceBefore && toks[i - 1].kind != Tok::EndOfStmt) s += ' ';
    if (t.kind == Tok::String) {
      s += quoteString(t.text);
    } else if (t.kind == Tok::EndOfStmt) {
      s += '\n';
    } else if (t.kind == Tok::BlockBody) { // the codeblock's statements on one line, separated by ;
      std::string body = render(blockBodies_.at((size_t)t.ival));
      for (size_t at = 0, end; at < body.size(); at = end + 1) {
        end = body.find('\n', at);
        if (end == std::string::npos) end = body.size();
        s += (at ? "; " : " ") + body.substr(at, end - at);
      }
    } else {
      s += t.text;
    }
  }
  return s;
}

} // namespace xb
