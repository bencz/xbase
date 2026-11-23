#pragma once

#include "xir/IR.h"
#include "xbclass.h" // the rules of classes the runtime applies (runtime/src/types/members.c)

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xb::xir {

// The module's classes as the runtime sees them once they are prepared: each class's line and what a message reaches,
// by the runtime's own rules (xbclass.h), on xb_class_def descriptions made from Module::classes. A class whose line
// reaches a class found by name when the program runs (a parent of another module, or one not declared) has no known
// line: nothing about its messages is known.
class ClassModel {
public:
  explicit ClassModel(const Module &m);
  ClassModel(const ClassModel &) = delete;
  ClassModel &operator=(const ClassModel &) = delete;

  // The rule of FINAL, ABSTRACT or IMPLEMENTS class `k` breaks (xb_class_check, as the runtime applies it), as far as
  // the module knows its line and interfaces: empty when none (or the line is not known).
  std::string check(int32_t k) const;
  // The line of class `k` (the class, then its ancestors), or null when it is not known.
  const std::vector<const xb_class_def *> *line(int32_t k) const;
  // What `message` reaches in an object of class `k` (whose line is known), as `lookup` finds it, or nothing.
  std::optional<xb_member> lookup(int32_t k, const std::string &message) const;
  // What a send reaches in an object of class `k` (exactly), sent from a method of class `from` (-1: elsewhere), as
  // Super:message with `super`: the member when the send reaches it and its scope lets the sender reach it, or nothing
  // (a cast, New or Init without a method, a message not answered, a scope only a friend passes, a line not known:
  // the runtime decides then).
  std::optional<xb_member> resolve(int32_t k, int32_t from, bool super, const std::string &message) const;
  // Whether the runtime answers `message` to an object of class `k` by itself: no member answers it and it names no
  // class of the line (which would make it a cast).
  bool answersByDefault(int32_t k, const std::string &message) const;
  int32_t indexOf(const xb_class_def *d) const;
  // The method a member names, a function of the module.
  Function *method(const xb_member &m) const;
  // The slot of an instance variable member in an object of class `k`.
  uint32_t slot(int32_t k, const xb_member &m) const;

private:
  struct Class {
    xb_class_def def{};
    std::vector<xb_class_parent> parents;
    std::vector<xb_class_var_def> vars, classVars;
    std::vector<xb_class_method_def> methods;
    std::vector<xb_class_parent> interfaces;
    std::vector<const xb_class_def *> line;
    bool known = false;
  };
  bool kindOf(int32_t k, const xb_class_def *x) const;
  void buildLine(int32_t k, std::vector<int32_t> &visiting);

  const Module &module_;
  std::vector<Class> classes_;
};

// What no class narrows: any object, or a value that is no object.
constexpr int32_t kNoClass = -2;

class CFG;

// The class each value of a function holds when it holds objects of one class of the module whose line is known:
// the object class.new makes, the object a New the runtime answers by itself returns (it sends Init and returns the
// object; not when a class of the line is named New: a cast), a parameter of a copy typed for it
// (Function::paramClasses), and a block parameter all of whose arguments hold objects of one class (an optimistic
// fixpoint: block parameters start with none and grow with what their branches pass).
class ClassInfo {
public:
  ClassInfo(const Function &f, const CFG &cfg, const ClassModel &model);
  // The class of `v`, or kNoClass.
  int32_t of(const Value *v) const {
    auto it = classes_.find(v);
    return it == classes_.end() ? kNoClass : it->second;
  }

private:
  int32_t compute(const Function &f, const Instruction &i) const;
  bool merge(const Value *v, int32_t c);

  const ClassModel &model_;
  std::unordered_map<const Value *, int32_t> classes_;
};

} // namespace xb::xir
