#pragma once

#include "xir/Analysis.h"
#include "xir/IR.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xb {

// Where an item value of an xIR function lives (docs/xir.md, phase 1).
struct Home {
  enum class Kind : uint8_t {
    None,     // not an item, or never materialized
    Constant, // a static literal
    Slot,     // a frame slot (a GC root)
    Place,    // the item of a variable: an elided load, or a value computed into the variable of the store using it
    Ret,      // the caller's result slot: the value returned by the `ret` using it
  };
  Kind kind = Kind::None;
  uint32_t slot = 0;                       // Slot
  const xir::Instruction *place = nullptr; // Place: the load or the store whose variable holds the value
};

// The frame of a function: the parameters in slots 0 .. numParams-1 (the
// prologue copies them there), then the other frame variables still in memory
// (those mem2reg did not make values), then temporaries. Every live item value that needs storage gets a slot; slots
// are assigned by linear scan over live intervals and reused once free.
// Operands of a call or array literal take consecutive slots (a group), and a
// value used only there is computed straight into its slot of the group; a
// branch argument used only there shares the slot of the block parameter; a
// load whose single use follows with nothing in between that can write the
// variable uses the variable's own item; a value used only by the store that
// follows it, or by `ret`, is computed in place. A Place home's address (a
// cell, an element reached through a reference) stays valid only because
// nothing between its computation and its use runs user code or grows an
// array, and objects never move; a moving collector must revisit it.
class FramePlan {
public:
  explicit FramePlan(const xir::Function &f);

  const xir::CFG &cfg() const { return cfg_; }
  uint32_t frameSize() const { return frameSize_; }
  uint32_t slotOf(const xir::Var &v) const { return varSlot_.at(&v); } // a frame variable
  const Home &home(const xir::Value *v) const;
  uint32_t groupBase(const xir::Instruction *user) const { return groupBase_.at(user); }
  bool inGroup(const xir::Instruction *user, size_t operand) const; // computed straight into its group slot
  bool forwarded(const xir::Instruction *store) const { return forwardedStores_.count(store) != 0; }

private:
  struct Unit {                  // what one allocation serves
    uint32_t start = UINT32_MAX, end = 0;
    uint32_t size = 1;           // slots (a group has one per operand)
    uint32_t slot = 0;
  };

  void placeVariables();
  void number();
  void chooseHomes();
  void elideLoads(const xir::BasicBlock &b);
  bool canForward(const xir::Instruction &value, const xir::Instruction &store) const;
  void computeLiveness();
  void allocate();
  uint32_t unitOf(const xir::Value *v);

  const xir::Function &f_;
  xir::CFG cfg_;
  uint32_t frameSize_ = 0;
  uint32_t numVarSlots_ = 0;
  std::unordered_map<const xir::Var *, uint32_t> varSlot_;
  std::unordered_map<const xir::Value *, uint32_t> pos_;          // definition position
  std::unordered_map<const xir::BasicBlock *, uint32_t> start_, end_, blockIndex_;
  std::unordered_map<const xir::Value *, Home> homes_;
  std::unordered_map<const xir::Value *, uint32_t> unit_;          // values in slots -> their unit
  std::unordered_map<const xir::Value *, uint32_t> groupMember_;   // value -> operand index in its group
  std::unordered_map<const xir::Instruction *, uint32_t> groupUnit_, groupBase_;
  std::unordered_set<const xir::Instruction *> forwardedStores_;
  std::vector<Unit> units_;
};

} // namespace xb
