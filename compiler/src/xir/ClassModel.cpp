#include "xir/ClassModel.h"

#include "xir/Analysis.h"

#include <algorithm>

namespace xb::xir {

ClassModel::ClassModel(const Module &m) : module_(m), classes_(m.classes.size()) {
  for (size_t k = 0; k < m.classes.size(); ++k) { // every description in place first: parents point at them
    const Module::Class &from = m.classes[k];
    Class &c = classes_[k];
    for (const auto &p : from.parents)
      c.parents.push_back({p.name.c_str(), p.local >= 0 ? &classes_[(size_t)p.local].def : nullptr});
    for (const auto &v : from.vars) c.vars.push_back({v.name.c_str(), v.flags});
    for (const auto &v : from.classVars) c.classVars.push_back({v.name.c_str(), v.flags});
    for (const auto &mt : from.methods) c.methods.push_back({mt.message.c_str(), nullptr, mt.flags, 0, nullptr, 0});
    for (const auto &p : from.interfaces)
      c.interfaces.push_back({p.name.c_str(), p.local >= 0 ? &classes_[(size_t)p.local].def : nullptr});
    c.def.name = from.name.c_str();
    c.def.parents = c.parents.data();
    c.def.nparents = (uint32_t)c.parents.size();
    c.def.vars = c.vars.data();
    c.def.nvars = (uint32_t)c.vars.size();
    c.def.methods = c.methods.data();
    c.def.nmethods = (uint32_t)c.methods.size();
    c.def.classVars = c.classVars.data();
    c.def.nclassVars = (uint32_t)c.classVars.size();
    c.def.interfaces = c.interfaces.data();
    c.def.ninterfaces = (uint32_t)c.interfaces.size();
    c.def.flags = from.kind | (from.scalar ? XB_CLASS_SCALAR : 0u);
  }
  std::vector<int32_t> visiting;
  for (size_t k = 0; k < classes_.size(); ++k) buildLine((int32_t)k, visiting);
}

// The line as the runtime makes it: the class, then each parent's line (xb_line_merge); not known when a parent is
// found by name when the program runs, or the class inherits from itself (an error then).
void ClassModel::buildLine(int32_t k, std::vector<int32_t> &visiting) {
  Class &c = classes_[(size_t)k];
  if (c.known || !c.line.empty() || std::find(visiting.begin(), visiting.end(), k) != visiting.end()) return;
  visiting.push_back(k);
  std::vector<const xb_class_def *> line{&c.def};
  bool known = true;
  for (const auto &p : module_.classes[(size_t)k].parents) {
    if (p.local < 0) {
      known = false;
      break;
    }
    buildLine(p.local, visiting);
    const Class &parent = classes_[(size_t)p.local];
    if (!parent.known) {
      known = false;
      break;
    }
    uint32_t n = (uint32_t)line.size();
    line.resize(n + parent.line.size());
    xb_line_merge(line.data(), &n, parent.line.data(), (uint32_t)parent.line.size());
    line.resize(n);
  }
  visiting.pop_back();
  if (known) c.line = std::move(line);
  c.known = known;
}

std::string ClassModel::check(int32_t k) const {
  const std::vector<const xb_class_def *> *l = line(k);
  if (!l || (classes_[(size_t)k].def.flags & XB_CLASS_INTERFACE)) return "";
  // The module's own interfaces have their defs already (Generator); another module's are the runtime's to check.
  auto find = [](void *, const char *) -> const xb_class_def * { return nullptr; };
  char why[512];
  if (!xb_class_check(l->data(), (uint32_t)l->size(), find, nullptr, 0, why, sizeof why)) return "";
  return why;
}

const std::vector<const xb_class_def *> *ClassModel::line(int32_t k) const {
  if (k < 0 || (size_t)k >= classes_.size() || !classes_[(size_t)k].known) return nullptr;
  return &classes_[(size_t)k].line;
}

std::optional<xb_member> ClassModel::lookup(int32_t k, const std::string &message) const {
  const std::vector<const xb_class_def *> *l = line(k);
  xb_member m;
  if (!l || !xb_line_lookup(l->data(), (uint32_t)l->size(), message.c_str(), &m)) return std::nullopt;
  return m;
}

int32_t ClassModel::indexOf(const xb_class_def *d) const {
  for (size_t k = 0; k < classes_.size(); ++k)
    if (&classes_[k].def == d) return (int32_t)k;
  return -1;
}

bool ClassModel::kindOf(int32_t k, const xb_class_def *x) const {
  const std::vector<const xb_class_def *> *l = line(k);
  return l && std::find(l->begin(), l->end(), x) != l->end();
}

namespace {

// What a class answers, for the shared rules' lookup callback: kept here for the pointers they hand back.
struct Lookups {
  const ClassModel *model;
  std::vector<xb_member> found;
};

const xb_member *lookupFor(void *data, const xb_class_def *c, const char *message) {
  auto *self = static_cast<Lookups *>(data);
  std::optional<xb_member> r = self->model->lookup(self->model->indexOf(c), message);
  if (!r) return nullptr;
  self->found.push_back(*r);
  return &self->found.back();
}

} // namespace

std::optional<xb_member> ClassModel::resolve(int32_t k, int32_t from, bool super, const std::string &message) const {
  if (!line(k) || (from >= 0 && !line(from))) return std::nullopt;
  const xb_class_def *f = from >= 0 ? &classes_[(size_t)from].def : nullptr;
  bool related = f && kindOf(k, f); // as the runtime's dispatch: Self is of the method's class
  if (super && !related) return std::nullopt; // the runtime's error
  Lookups lookups{this, {}};
  lookups.found.reserve(classes_.size() + 2); // the pointers lookupFor hands back stay valid
  const std::vector<const xb_class_def *> *fl = f ? line(from) : nullptr;
  const xb_member *m = xb_member_reach(&classes_[(size_t)k].def, f, fl ? fl->data() : nullptr,
                                       fl ? (uint32_t)fl->size() : 0, super ? 1 : 0, related ? 1 : 0, message.c_str(),
                                       lookupFor, &lookups);
  if (!m) return std::nullopt;
  bool inside = from >= 0 && kindOf(from, m->owner);
  if (!xb_member_scope_ok(m, f, inside ? 1 : 0)) return std::nullopt; // a friend may: the runtime decides
  return *m;
}

bool ClassModel::answersByDefault(int32_t k, const std::string &message) const {
  const std::vector<const xb_class_def *> *l = line(k);
  return l && !lookup(k, message) && xb_line_named(l->data(), (uint32_t)l->size(), message.c_str()) == l->size();
}

Function *ClassModel::method(const xb_member &m) const {
  int32_t owner = indexOf(m.owner);
  return owner < 0 ? nullptr : module_.classes[(size_t)owner].methods[m.index].fn;
}

uint32_t ClassModel::slot(int32_t k, const xb_member &m) const {
  const std::vector<const xb_class_def *> &l = *line(k);
  auto at = (uint32_t)(std::find(l.begin(), l.end(), m.owner) - l.begin());
  return xb_line_area(l.data(), at) + m.index;
}

ClassInfo::ClassInfo(const Function &f, const CFG &cfg, const ClassModel &model) : model_(model) {
  for (bool changed = true; changed;) {
    changed = false;
    for (BasicBlock *b : cfg.order())
      for (auto &ip : b->insts) {
        const Instruction &i = *ip;
        if (i.type() == Type::Item) changed |= merge(&i, compute(f, i));
        for (const Target &t : i.targets)
          for (size_t k = 0; k < t.args.size(); ++k)
            if (t.block->params[k]->type() == Type::Item) changed |= merge(t.block->params[k].get(), of(t.args[k]));
      }
  }
}

int32_t ClassInfo::compute(const Function &f, const Instruction &i) const {
  switch (i.op) {
  case Op::ClassNew: return model_.line(i.classIndex) ? i.classIndex : kNoClass;
  case Op::Send: { // New without a method of the class: the object (Init runs; an error leaves instead)
    int32_t c = i.operands().empty() ? kNoClass : of(i.operand(0));
    if (c == kNoClass || i.superSend || i.message != "NEW" || !model_.answersByDefault(c, "NEW")) return kNoClass;
    return c;
  }
  case Op::Load:
    if (isParameter(i.var) && i.var->index < f.paramClasses.size() && !i.var->shared && i.access == Access::Direct)
      return f.paramClasses[i.var->index];
    return kNoClass;
  default: return kNoClass;
  }
}

// Joins `c` into what `v` holds: true when that changed.
bool ClassInfo::merge(const Value *v, int32_t c) {
  auto [it, fresh] = classes_.emplace(v, c);
  if (fresh) return true;
  if (it->second == c || it->second == kNoClass) return false;
  it->second = kNoClass;
  return true;
}

} // namespace xb::xir
