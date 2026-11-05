// Building a rule's result from the captured match markers.
#include "preprocessor/Preprocessor.h"

#include "preprocessor/Internal.h"

#include <algorithm>

namespace xb {

using namespace pp;

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

} // namespace xb
