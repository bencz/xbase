// The pass pipeline on the syntax tree, its verifier and its printer (opt/AstPasses.h).
#include "opt/AstPasses.h"

#include "parser/Walk.h"

#include <string>

namespace xb {

namespace {

struct AstPass {
  const char *name;
  bool (*run)(Module &m);
};

constexpr AstPass kAstPipeline[] = {
    {"local-tables", markLocalTables},
    {"value-captures", markValueCaptures},
};

// The frames a node is in, innermost last: the function's (no block), then each codeblock's.
struct Frames {
  const FuncDecl *f = nullptr;
  std::vector<const BlockExpr *> blocks;
  uint32_t size() const { return blocks.empty() ? f->numLocals : blocks.back()->numLocals; }
};

class Verifier {
public:
  explicit Verifier(const Module &m) : m_(m) {}

  std::string run() {
    for (auto &f : m_.funcs) {
      at_ = Frames{f.get(), {}};
      if (f->capturedByValue.size() > f->numLocals) fail(f->name + ": capturedByValue past its slots");
      if (f->boxed.size() > f->numLocals) fail(f->name + ": boxed past its slots");
      block(const_cast<Block &>(f->body));
    }
    return report_;
  }

private:
  void fail(const std::string &what) {
    if (report_.empty()) report_ = what;
  }

  void block(Block &b) {
    for (auto &s : b) forEachPart(*s, [&](ExprPtr &e) { expr(*e); }, [&](Block &x) { block(x); });
  }

  void var(const VarExpr &v) {
    std::string where = at_.f->name + ": " + v.name;
    switch (v.where) {
    case VarWhere::Frame:
      if (v.slot >= at_.size()) fail(where + " past its frame's slots");
      break;
    case VarWhere::Capture:
      if (at_.blocks.empty() || v.slot >= at_.blocks.back()->captures.size()) fail(where + " not a capture here");
      break;
    case VarWhere::Static:
      if (v.slot >= m_.statics.size()) fail(where + " past the module's STATICs");
      break;
    case VarWhere::Memvar: case VarWhere::Field: break;
    }
  }

  void expr(Expr &e) {
    if (e.kind == ExprKind::Var) var(static_cast<const VarExpr &>(e));
    if ((e.kind == ExprKind::Assign && !isLvalue(*static_cast<AssignExpr &>(e).target)) ||
        (e.kind == ExprKind::IncDec && !isLvalue(*static_cast<IncDecExpr &>(e).target)))
      fail(at_.f->name + ": an assignment to something that is no variable");
    bool isBlock = e.kind == ExprKind::Block;
    if (isBlock) {
      auto &b = static_cast<const BlockExpr &>(e);
      for (const BlockExpr::Capture &cap : b.captures) {
        uint32_t room = cap.fromParentCapture ? (at_.blocks.empty() ? 0 : (uint32_t)at_.blocks.back()->captures.size())
                                              : at_.size();
        if (cap.index >= room) fail(at_.f->name + ": a codeblock captures past what its parent has");
      }
      at_.blocks.push_back(&b);
    }
    forEachChildExpr(e, [&](ExprPtr &c) { expr(*c); });
    forEachChildBlock(e, [&](Block &x) { block(x); });
    if (isBlock) at_.blocks.pop_back();
  }

  const Module &m_;
  Frames at_;
  std::string report_;
};

const char *binOpName(BinOp op) {
  static const char *names[] = {"add", "sub", "mul", "div", "mod", "pow", "eq", "exact.eq", "ne", "lt", "le", "gt",
                                "ge", "in", "bit.and", "bit.or", "bit.xor", "shl", "shr"};
  return names[static_cast<size_t>(op)];
}

const char *unOpName(UnOp op) {
  static const char *names[] = {"neg", "plus", "not", "bit.not"};
  return names[static_cast<size_t>(op)];
}

const char *whereName(VarWhere w) {
  static const char *names[] = {"frame", "capture", "static", "memvar", "field"};
  return names[static_cast<size_t>(w)];
}

const char *stmtName(StmtKind k) {
  static const char *names[] = {"expr",  "local",  "static",   "private", "public",      "parameters", "memvar",
                                "field", "return", "if",       "while",   "for",         "for.each",   "switch",
                                "sequence", "exit", "loop",    "with.object", "yield",   "using",      "await.each"};
  return names[static_cast<size_t>(k)];
}

class Printer {
public:
  std::string run(const Module &m) {
    line(0, "module " + m.name);
    for (size_t k = 0; k < m.statics.size(); ++k) line(1, "static " + std::to_string(k) + " " + m.statics[k]->name);
    for (auto &f : m.funcs) {
      std::string head = (f->isProcedure ? "procedure " : "function ") + f->name + " locals " +
                         std::to_string(f->numLocals);
      std::vector<bool> tables(f->numLocals);
      for (auto &st : f->body)
        if (st->kind == StmtKind::Local)
          for (const LocalDecl &d : static_cast<const LocalStmt &>(*st).decls)
            if (d.table && d.slot < tables.size()) tables[d.slot] = true;
      for (size_t s = 0; s < f->numLocals; ++s) {
        if (s < f->boxed.size() && f->boxed[s]) head += " shared:" + std::to_string(s);
        if (s < f->capturedByValue.size() && f->capturedByValue[s]) head += " by.value:" + std::to_string(s);
        if (tables[s]) head += " table:" + std::to_string(s);
      }
      line(1, head);
      block(f->body, 2);
    }
    return out_;
  }

private:
  void line(int depth, const std::string &text) {
    out_.append((size_t)depth * 2, ' ');
    out_ += text;
    out_ += '\n';
  }

  void block(const Block &b, int depth) {
    for (auto &s : b) {
      line(depth, stmtName(s->kind));
      forEachPart(const_cast<Stmt &>(*s), [&](ExprPtr &e) { expr(*e, depth + 1); },
                  [&](Block &x) {
                    line(depth + 1, "{");
                    block(x, depth + 2);
                    line(depth + 1, "}");
                  });
    }
  }

  static std::string label(const Expr &e) {
    switch (e.kind) {
    case ExprKind::Nil: return "nil";
    case ExprKind::Logical: return static_cast<const LogicalExpr &>(e).value ? ".T." : ".F.";
    case ExprKind::Number: {
      auto &n = static_cast<const NumberExpr &>(e);
      return "number " + (n.isInt ? std::to_string(n.ival) : std::to_string(n.dval)) + " w" + std::to_string(n.width) +
             (n.isInt ? "" : " d" + std::to_string(n.dec));
    }
    case ExprKind::String: return "string \"" + static_cast<const StringExpr &>(e).value + "\"";
    case ExprKind::Var: {
      auto &v = static_cast<const VarExpr &>(e);
      return "var " + v.name + " " + whereName(v.where) + " " + std::to_string(v.slot);
    }
    case ExprKind::Call: return "call " + static_cast<const CallExpr &>(e).name;
    case ExprKind::Unary: return std::string("unary ") + unOpName(static_cast<const UnaryExpr &>(e).op);
    case ExprKind::Binary: return std::string("binary ") + binOpName(static_cast<const BinaryExpr &>(e).op);
    case ExprKind::And: return "and";
    case ExprKind::Or: return "or";
    case ExprKind::Assign: {
      auto &a = static_cast<const AssignExpr &>(e);
      return a.compound ? std::string("assign ") + binOpName(a.op) : "assign";
    }
    case ExprKind::IncDec: {
      auto &i = static_cast<const IncDecExpr &>(e);
      return std::string(i.increment ? "inc" : "dec") + (i.prefix ? " prefix" : " postfix");
    }
    case ExprKind::IIf: return "iif";
    case ExprKind::Ref: return "ref";
    case ExprKind::List: return "list";
    case ExprKind::ArrayLit: return "array";
    case ExprKind::Index: return "index";
    case ExprKind::Block: {
      auto &b = static_cast<const BlockExpr &>(e);
      std::string s = "block params " + std::to_string(b.params.size()) + " locals " + std::to_string(b.numLocals);
      for (const BlockExpr::Capture &c : b.captures)
        s += (c.fromParentCapture ? " capture:" : " frame:") + std::to_string(c.index);
      return s;
    }
    case ExprKind::Send: {
      auto &s = static_cast<const SendExpr &>(e);
      return "send " + (s.message.empty() ? std::string("&") : s.message) + (s.super ? " super" : "");
    }
    case ExprKind::HashLit: return "hash";
    case ExprKind::Spread: return "spread";
    case ExprKind::Macro: return "macro";
    case ExprKind::MacroCall: return "macro.call";
    case ExprKind::MacroText: return "macro.text \"" + static_cast<const MacroTextExpr &>(e).value + "\"";
    case ExprKind::Field: {
      auto &f = static_cast<const FieldExpr &>(e);
      return "field " + (f.alias.empty() ? std::string("") : f.alias + "->") + f.name;
    }
    case ExprKind::Await: return "await";
    }
    return "?";
  }

  void expr(Expr &e, int depth) {
    line(depth, label(e));
    forEachChildExpr(e, [&](ExprPtr &c) { expr(*c, depth + 1); });
    forEachChildBlock(e, [&](Block &x) { block(x, depth + 1); });
  }

  std::string out_;
};

} // namespace

std::string verifyAst(const Module &m) { return Verifier(m).run(); }

std::string printAst(const Module &m) { return Printer().run(m); }

std::string optimizeAst(Module &m) {
  for (const AstPass &p : kAstPipeline) {
    if (!p.run(m)) continue;
    if (std::string report = verifyAst(m); !report.empty()) return std::string("after ") + p.name + ": " + report;
  }
  return "";
}

} // namespace xb
