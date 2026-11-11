// Text form of xIR, for --dump-xir and tests. The compiler never reads it.
#include "xir/Printer.h"

#include <cstdio>

namespace xb::xir {

namespace {

const char *binopName(BinOp op) {
  switch (op) {
  case BinOp::Add: return "add";
  case BinOp::Sub: return "sub";
  case BinOp::Mul: return "mul";
  case BinOp::Div: return "div";
  case BinOp::Mod: return "mod";
  case BinOp::Pow: return "pow";
  case BinOp::Eq: return "eq";
  case BinOp::ExactEq: return "exacteq";
  case BinOp::NotEq: return "ne";
  case BinOp::Lt: return "lt";
  case BinOp::Le: return "le";
  case BinOp::Gt: return "gt";
  case BinOp::Ge: return "ge";
  case BinOp::In: return "in";
  case BinOp::BitAnd: return "bitand";
  case BinOp::BitOr: return "bitor";
  case BinOp::BitXor: return "bitxor";
  case BinOp::Shl: return "shl";
  case BinOp::Shr: return "shr";
  }
  return "?";
}

const char *predName(IntPred p) {
  switch (p) {
  case IntPred::Eq: return "eq";
  case IntPred::Ne: return "ne";
  case IntPred::Lt: return "lt";
  case IntPred::Le: return "le";
  case IntPred::Gt: return "gt";
  case IntPred::Ge: return "ge";
  }
  return "?";
}

const char *accessName(Access a) {
  switch (a) {
  case Access::Direct: return "";
  case Access::ThroughRef: return " ref";
  case Access::MaybeRef: return " maybe_ref";
  }
  return "";
}

std::string quoted(const std::string &s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    if ((unsigned char)c < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\x%02x", (unsigned)(unsigned char)c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

class Printer {
public:
  explicit Printer(std::string &out) : out_(out) {}

  void function(const Function &f) {
    f.renumber();
    static const char *const kinds[] = {"function", "block", "module_init"};
    out_ += std::string(kinds[(int)f.kind]) + " " + f.name + (f.internal ? " internal" : "") +
            " params " + std::to_string(f.numParams) + (f.readsArgs ? " args" : "") + (f.method ? " method" : "") +
            (f.callsPrivates ? " privates" : "") + " {\n";
    for (auto &v : f.vars) out_ += "  " + varName(*v) + " = " + varDesc(*v) + "\n";
    for (auto &b : f.blocks) block(*b);
    out_ += "}\n";
  }

  void module(const Module &m) {
    out_ += "module " + m.name + "\n";
    for (const std::string &r : m.requests) out_ += "  request " + r + "\n";
    for (auto &s : m.statics) out_ += "  " + varName(*s) + " = " + varDesc(*s) + "\n";
    for (auto &f : m.functions) {
      out_ += "\n";
      function(*f);
    }
  }

private:
  static std::string varName(const Var &v) {
    switch (v.kind) {
    case Var::Kind::Frame: return "$f" + std::to_string(v.index);
    case Var::Kind::Capture: return "$c" + std::to_string(v.index);
    case Var::Kind::Static: return "$s" + std::to_string(v.index);
    }
    return "$?";
  }

  static std::string varDesc(const Var &v) {
    static const char *const kinds[] = {"frame", "capture", "static"};
    return std::string(kinds[(int)v.kind]) + " " + std::to_string(v.index) + (v.shared ? " shared" : "") + " " +
           quoted(v.name);
  }

  std::string value(const Value *v) {
    if (v->kind() != Value::Kind::Constant) return "%" + std::to_string(v->id);
    auto *c = static_cast<const Constant *>(v);
    switch (c->lit) {
    case Constant::Lit::Nil: return "nil";
    case Constant::Lit::Logical: return c->logical ? ".T." : ".F.";
    case Constant::Lit::Integer: return std::to_string(c->ival) + ":w" + std::to_string(c->width);
    case Constant::Lit::Double: {
      char buf[64];
      std::snprintf(buf, sizeof buf, "%.17g:w%u:d%u", c->dval, (unsigned)c->width, (unsigned)c->dec);
      return buf;
    }
    case Constant::Lit::String: return quoted(c->str);
    case Constant::Lit::Bool: return c->logical ? "true" : "false";
    case Constant::Lit::I64: return "i64 " + std::to_string(c->ival);
    }
    return "?";
  }

  std::string target(const Target &t) {
    std::string s = t.block->name;
    if (t.args.empty()) return s;
    s += "(";
    for (size_t i = 0; i < t.args.size(); ++i) s += (i ? ", " : "") + value(t.args[i]);
    return s + ")";
  }

  void block(const BasicBlock &b) {
    out_ += b.name;
    if (!b.params.empty()) {
      out_ += "(";
      for (size_t i = 0; i < b.params.size(); ++i)
        out_ += std::string(i ? ", " : "") + "%" + std::to_string(b.params[i]->id) + ": " +
                typeName(b.params[i]->type());
      out_ += ")";
    }
    if (b.unwind) out_ += " unwind " + b.unwind->name;
    out_ += ":\n";
    for (auto &i : b.insts) inst(*i);
  }

  void inst(const Instruction &i) {
    out_ += "  ";
    if (i.type() != Type::Void) out_ += "%" + std::to_string(i.id) + " = ";
    out_ += opName(i.op);
    switch (i.op) {
    case Op::Unary: out_ += i.unop == UnOp::Neg ? " neg" : i.unop == UnOp::BitNot ? " bitnot" : " not"; break;
    case Op::Binary: out_ += std::string(" ") + binopName(i.binop) + (i.byOne ? ".one" : ""); break;
    case Op::IntCmp: out_ += std::string(" ") + predName(i.pred); break;
    case Op::NumDefaultDec: {
      char buf[48];
      std::snprintf(buf, sizeof buf, " %.17g", i.imm);
      out_ += buf;
      break;
    }
    case Op::Call:
      out_ += " " + (i.callee.function ? i.callee.function->name : i.callee.name) +
              (i.callee.function ? "" : i.callee.native ? " native" : " extern");
      break;
    case Op::Send:
      out_ += " " + i.message;
      if (i.classIndex >= 0) out_ += std::string(i.superSend ? " super " : " from ") + std::to_string(i.classIndex);
      break;
    case Op::MvGet: case Op::MvSet: case Op::MvRef: case Op::MvPrivate: case Op::MvPublic:
      out_ += " " + i.message;
      break;
    case Op::ClassNew: out_ += " " + std::to_string(i.classIndex); break;
    case Op::MvParam: out_ += " " + i.message + " " + std::to_string(i.index); break;
    case Op::MakeBlock:
      out_ += " " + i.blockFn->name;
      for (Var *c : i.captures) out_ += " " + varName(*c);
      break;
    case Op::ParamsRef:
      for (Var *c : i.captures) out_ += " " + varName(*c);
      break;
    case Op::CallForward: case Op::GenNew: out_ += " " + i.callee.function->name; break;
    default:
      break;
    }
    if (i.var) out_ += " " + varName(*i.var) + accessName(i.access);
    const auto &ops = i.operands();
    for (size_t k = 0; k < ops.size(); ++k) out_ += (k || i.var ? ", " : " ") + value(ops[k]);
    if (i.op == Op::Br) out_ += " " + target(i.targets[0]);
    if (i.op == Op::GenYield) out_ += ", " + std::to_string(i.index) + ", " + target(i.targets[0]);
    if (i.op == Op::CondBr) out_ += ", " + target(i.targets[0]) + ", " + target(i.targets[1]);
    out_ += "\n";
  }

  std::string &out_;
};

} // namespace

std::string print(const Module &m) {
  std::string out;
  Printer(out).module(m);
  return out;
}

std::string print(const Function &f) {
  std::string out;
  Printer(out).function(f);
  return out;
}

} // namespace xb::xir
