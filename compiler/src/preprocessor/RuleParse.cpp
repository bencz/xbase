// Parsing #command / #translate rules: match patterns and result patterns.
#include "preprocessor/Preprocessor.h"

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

} // namespace xb
