// Parsing #command / #translate rules: match patterns and result patterns.
#include "preprocessor/Preprocessor.h"

#include <algorithm>
#include <cstring>

#include "preprocessor/Internal.h"
#include "common/Text.h"

#include <cstdint>

namespace xb {

using namespace pp;

int Preprocessor::findMarker(const Rule &r, const std::string &name) const {
  for (size_t i = 0; i < r.markers.size(); ++i)
    if (r.markers[i].name == name) return (int)i;
  return -1;
}

// The operators made of < > and = (<= >= <> << >> <<= >>=) are one token
// each, which would swallow the bracket of a marker next to them (`<a><<b>`,
// `<n>>2`, `<x,...>>1`, `<a>=<b>`). In a rule they are split into their
// characters before the markers are found; the characters left as literals
// next to each other are joined again afterwards.
static void splitAngleOperators(std::vector<Token> &line) {
  for (size_t i = 0; i < line.size(); ++i) {
    Token &t = line[i];
    if (t.text.size() < 2 || (t.text[0] != '<' && t.text[0] != '>') || t.kind == Tok::String) continue;
    std::vector<Token> chars;
    for (size_t c = 0; c < t.text.size(); ++c) {
      Token ch = t;
      ch.text = t.text.substr(c, 1);
      ch.kind = ch.text == "<" ? Tok::Lt : ch.text == ">" ? Tok::Gt : Tok::Eq;
      ch.loc.col += (uint32_t)c;
      if (c) ch.spaceBefore = false;
      chars.push_back(std::move(ch));
    }
    line.erase(line.begin() + (long)i);
    line.insert(line.begin() + (long)i, chars.begin(), chars.end());
    i += chars.size() - 1;
  }
}

// Joins literal tokens of single characters written together (`<` `<` is
// `<<`), the longest operator first, in a rule's nodes and their clauses.
template <class Node> static void joinAngleOperators(std::vector<Node> &nodes) {
  static const std::pair<const char *, Tok> ops[] = {{"<<=", Tok::ShlEq}, {">>=", Tok::ShrEq}, {"<<", Tok::Shl},
                                                     {">>", Tok::Shr},    {"<=", Tok::Le},      {">=", Tok::Ge},
                                                     {"<>", Tok::NotEq}};
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].k != Node::Lit || (nodes[i].tok.kind != Tok::Lt && nodes[i].tok.kind != Tok::Gt)) continue;
    std::string text = nodes[i].tok.text;
    size_t n = 1;
    while (n < 3 && i + n < nodes.size() && nodes[i + n].k == Node::Lit && !nodes[i + n].tok.spaceBefore &&
           nodes[i + n].tok.text.size() == 1 && std::string("<>=").find(nodes[i + n].tok.text[0]) != std::string::npos)
      text += nodes[i + n++].tok.text;
    for (auto [op, kind] : ops) {
      size_t len = std::strlen(op);
      if (len > n || text.compare(0, len, op) != 0) continue;
      nodes[i].tok.kind = kind;
      nodes[i].tok.text = op;
      nodes.erase(nodes.begin() + (long)i + 1, nodes.begin() + (long)(i + len));
      break;
    }
  }
  for (Node &node : nodes) joinAngleOperators(node.kids);
}

bool Preprocessor::parseRuleParts(Toks &line, Rule &r, bool withResult) {
  splitAngleOperators(line);
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
  if (!parsePattern(line, 2, arrow, r, r.pattern)) return false;
  if (r.pattern.empty() || r.pattern.front().k != PNode::Lit) {
    diag_.error(line[0].loc, "rule pattern must start with a literal token");
    return false;
  }
  Toks pat(line.begin() + 2, line.begin() + (long)arrow);
  r.patternKey = upperCase(render(pat));
  if (withResult && !parseResult(line, arrow + 1, line.size(), r, r.result)) return false;
  joinAngleOperators(r.pattern);
  joinAngleOperators(r.result);
  // A marker can take several values (its optional clause matched more than
  // once) only if every use of it in the result is inside an optional result
  // clause, which repeats for each value; one used elsewhere has room for one.
  for (Marker &m : r.markers) m.repeatable = true;
  for (const RNode &n : r.result)
    if (n.k == RNode::Mark) r.markers[(size_t)n.marker].repeatable = false;
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

bool Preprocessor::parseMatchMarker(const Toks &t, size_t &i, size_t to, Rule &r, PNode &node) {
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
  node.k = PNode::Mark;
  node.marker = (int)r.markers.size();
  node.tok = t[i];
  r.markers.push_back(std::move(m));
  i = next;
  return true;
}

bool Preprocessor::parsePattern(const Toks &t, size_t from, size_t to, Rule &r, std::vector<PNode> &out) {
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
      n.tok = tk;
      if (!parsePattern(t, i + 1, close, r, n.kids)) return false;
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
      if (parseMatchMarker(t, i, to, r, n)) {
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
  // In a run of optional clauses, one without a keyword goes after the next
  // one with a keyword, so it never takes that keyword as an expression
  // (`[<a>] [TO <b>]` matches like `[TO <b>] [<a>]`); two without keywords in
  // a row are ambiguous.
  // A keyword at the clause's level: a literal token, or a restricted marker
  // (it only takes its listed words).
  auto hasKeyword = [&r](const std::vector<PNode> &kids) {
    return std::any_of(kids.begin(), kids.end(), [&r](const PNode &n) {
      return n.k == PNode::Lit || (n.k == PNode::Mark && r.markers[(size_t)n.marker].kind == MK::Restricted);
    });
  };
  size_t last = SIZE_MAX; // the previous optional clause of the run
  for (size_t k = 0; k < out.size(); ++k) {
    if (out[k].k != PNode::Opt) {
      last = SIZE_MAX;
      continue;
    }
    if (last != SIZE_MAX && !hasKeyword(out[last].kids)) {
      if (!hasKeyword(out[k].kids)) {
        diag_.error(out[k].tok.loc, "ambiguous rule pattern: two optional clauses in a row without a keyword");
        return false;
      }
      std::swap(out[last].kids, out[k].kids);
    }
    last = k;
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

} // namespace xb
