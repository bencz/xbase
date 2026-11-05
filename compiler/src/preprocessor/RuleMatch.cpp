// Matching a statement against a rule's pattern.
#include "preprocessor/Preprocessor.h"

#include "preprocessor/Internal.h"
#include "common/Text.h"

namespace xb {

using namespace pp;

namespace {

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

} // namespace

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

} // namespace xb
