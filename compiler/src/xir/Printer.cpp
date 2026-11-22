// Text form of xIR, for --dump-xir and tests. The compiler never reads it.
#include "xir/Printer.h"

#include "xir/Analysis.h"
#include "xir/Types.h"

#include <cstdio>
#include <memory>

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

// An external function as listings show it: its name, `native` (the runtime's), `library L` (an xbase library's) or
// `extern` (another module's).
std::string calleeName(const Callee &c) {
  return c.name + (c.native ? " native" : !c.library.empty() ? " library " + c.library : " extern");
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
  Printer(std::string &out, bool types) : out_(out), withTypes_(types) {}

  void function(const Function &f) {
    f.renumber();
    std::unique_ptr<CFG> cfg;
    if (withTypes_ && !f.blocks.empty()) {
      cfg = std::make_unique<CFG>(f);
      types_ = std::make_unique<TypeInfo>(*cfg);
    }
    static const char *const kinds[] = {"function", "block", "module_init"};
    out_ += std::string(kinds[(int)f.kind]) + " " + f.name + (f.internal ? " internal" : "") +
            " params " + std::to_string(f.numParams) + (f.readsArgs ? " args" : "") + (f.method ? " method" : "") +
            (f.callsPrivates ? " privates" : "") + (f.runtime ? " runtime" : "") + " {\n";
    for (auto &v : f.vars) out_ += "  " + varName(*v) + " = " + varDesc(*v) + "\n";
    for (auto &b : f.blocks) block(*b);
    out_ += "}\n";
    types_.reset();
  }

  void module(const Module &m) {
    out_ += "module " + m.name + "\n";
    for (const Callee &r : m.requests) out_ += "  request " + calleeName(r) + "\n";
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
                typeName(b.params[i]->type()) + typesOf(b.params[i].get());
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
    case Op::Binary:
      out_ += std::string(" ") + binopName(i.binop) + (i.byOne ? ".one" : "") + (i.grows ? ".grow" : "");
      break;
    case Op::IntCmp: out_ += std::string(" ") + predName(i.pred); break;
    case Op::NumDefaultDec: {
      char buf[48];
      std::snprintf(buf, sizeof buf, " %.17g", i.imm);
      out_ += buf;
      break;
    }
    case Op::SubstCheck:
      if (i.imm != 0) out_ += " guard " + std::to_string((uint32_t)i.imm); // the types it lets on (TypeSet)
      break;
    case Op::Call: case Op::FuncRef:
      out_ += " " + (i.callee.function ? i.callee.function->name : calleeName(i.callee));
      break;
    case Op::Send: case Op::SendMacro:
      out_ += i.op == Op::Send ? " " + i.message : i.index ? std::string(" assign") : std::string();
      if (i.classIndex >= 0) out_ += std::string(i.superSend ? " super " : " from ") + std::to_string(i.classIndex);
      break;
    case Op::MvGet: case Op::MvField: case Op::MvSet: case Op::MvRef: case Op::MvPrivate: case Op::MvPublic:
      out_ += " " + i.message;
      break;
    case Op::FieldGet: case Op::FieldSet:
      out_ += " " + (i.alias.empty() ? std::string() : i.alias + "->") + i.message;
      break;
    case Op::ClassNew: case Op::ObjectIs: out_ += " " + std::to_string(i.classIndex); break;
    case Op::ObjectGet: case Op::ObjectSet: case Op::ScalarPlain: out_ += " " + std::to_string(i.index); break;
    case Op::FrameRelease:
      if (i.index) out_ += " param " + std::to_string(i.index - 1) + " of " + i.blockFn->name;
      break;
    case Op::MvParam: out_ += " " + i.message + " " + std::to_string(i.index); break;
    case Op::MakeBlock: case Op::InlineEnter:
      out_ += " " + i.blockFn->name;
      for (Var *c : i.captures) out_ += " " + varName(*c);
      break;
    case Op::ParamsRef:
      for (Var *c : i.captures) out_ += " " + varName(*c);
      break;
    case Op::CallForward: case Op::GenNew: case Op::AgenNew: case Op::AsyncNew:
      out_ += " " + i.callee.function->name;
      break;
    case Op::AwaitFuse: out_ += i.callee.function ? " " + i.callee.function->name : std::string(" send"); break;
    default:
      break;
    }
    if (i.var) out_ += " " + varName(*i.var) + accessName(i.access) + (i.moves ? " move" : "");
    const auto &ops = i.operands();
    for (size_t k = 0; k < ops.size(); ++k) out_ += (k || i.var ? ", " : " ") + value(ops[k]);
    if (i.op == Op::Br) out_ += " " + target(i.targets[0]);
    if (isSuspension(i.op)) out_ += ", " + std::to_string(i.index) + ", " + target(i.targets[0]);
    if (i.op == Op::CondBr) out_ += ", " + target(i.targets[0]) + ", " + target(i.targets[1]);
    if (i.type() == Type::Item) out_ += typesOf(&i);
    out_ += "\n";
  }

  static const char *tagName(xb_type t) {
    switch (t) {
    case XB_T_NIL: return "nil";
    case XB_T_LOGICAL: return "logical";
    case XB_T_INT: return "int";
    case XB_T_DOUBLE: return "double";
    case XB_T_STRING: return "string";
    case XB_T_ARRAY: return "array";
    case XB_T_BLOCK: return "block";
    case XB_T_CELL: return "cell";
    case XB_T_REF: return "ref";
    case XB_T_OBJECT: return "object";
    case XB_T_HASH: return "hash";
    case XB_T_DATE: return "date";
    case XB_T_SYMBOL: return "symbol";
    case XB_T_COUNT_: break;
    }
    return "?";
  }

  // " {int,double}": the types the inference finds an item value may hold; nothing without --dump-xir-types.
  std::string typesOf(const Value *v) const {
    if (!types_ || v->type() != Type::Item) return "";
    TypeSet t = types_->of(v);
    if (t == kAnyType) return " {any}";
    std::string s;
    for (int k = 0; k < XB_T_COUNT_; ++k)
      if (t & typeBit(xb_type(k))) s += std::string(s.empty() ? "" : ",") + tagName(xb_type(k));
    return " {" + s + "}";
  }

  std::string &out_;
  bool withTypes_;
  std::unique_ptr<TypeInfo> types_;
};

} // namespace

std::string print(const Module &m, bool types) {
  std::string out;
  Printer(out, types).module(m);
  return out;
}

std::string print(const Function &f, bool types) {
  std::string out;
  Printer(out, types).function(f);
  return out;
}

} // namespace xb::xir
