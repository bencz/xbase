// Codeblocks: their functions and their creation with the captured cells.
#include "codegen/CodeGen.h"

#include <llvm/IR/Constants.h>

using namespace llvm;

namespace xb {

CodeGen::FnState CodeGen::saveFn() {
  return {fn_, fnCtx_, fnRet_, fnBase_, fnSelf_, fnPush_, fnExit_, numLocals_, temps_, maxSlots_,
          std::move(loops_), std::move(eachVars_), fnBoxed_, b_.saveIP()};
}

void CodeGen::restoreFn(FnState s) {
  fn_ = s.fn;
  fnCtx_ = s.ctx;
  fnRet_ = s.ret;
  fnBase_ = s.base;
  fnSelf_ = s.self;
  fnPush_ = s.push;
  fnExit_ = s.exit;
  numLocals_ = s.numLocals;
  temps_ = s.temps;
  maxSlots_ = s.maxSlots;
  loops_ = std::move(s.loops);
  eachVars_ = std::move(s.eachVars);
  fnBoxed_ = s.boxed;
  b_.restoreIP(s.ip);
}

// A codeblock is a function (ctx, ret, argc, argv, self): its frame holds its
// parameters, and the variables it uses from enclosing functions or blocks
// are reached through self->cells[].
Function *CodeGen::genCodeblockFunction(const BlockExpr &blk) {
  std::string name = fn_->getName().str() + ".block" + std::to_string(++blockCount_);
  Function *bf = Function::Create(blockFnTy_, GlobalValue::InternalLinkage, name, mod_);
  addAbiAttributes(bf);
  bf->setUWTableKind(UWTableKind::Async);

  FnState outer = saveFn();
  fn_ = bf;
  b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", bf));
  fnCtx_ = bf->getArg(0);
  fnRet_ = bf->getArg(1);
  Value *argc = bf->getArg(2);
  Value *argv = bf->getArg(3);
  fnSelf_ = bf->getArg(4);
  static const std::vector<bool> noBoxed; // codeblock parameters are never captured
  beginFrame(argc, argv, blk.numLocals, blk.numLocals, noBoxed);
  for (size_t i = 0; i < blk.body.size(); ++i) {
    uint32_t mark = temps_;
    if (i + 1 < blk.body.size()) genDiscard(*blk.body[i]);
    else genExpr(*blk.body[i], fnRet_); // the caller has set the result to NIL
    temps_ = mark;
  }
  endFrame();

  restoreFn(std::move(outer));
  return bf;
}

// Creating a block stores it in `dst` and hands it the cells it captures;
// nothing can allocate between the creation and the last cell store.
void CodeGen::genCodeblock(const BlockExpr &blk, Value *dst) {
  Function *bf = genCodeblockFunction(blk);
  Value *block = b_.CreateCall(rtBlockNew_, {fnCtx_, dst, bf, ConstantInt::get(i32_, blk.captures.size())}, "block");
  for (uint32_t k = 0; k < blk.captures.size(); ++k) {
    const BlockExpr::Capture &cap = blk.captures[k];
    Value *cell = cap.fromParentCapture ? b_.CreateLoad(ptrTy_, capturedCellAddress(fnSelf_, cap.index), "cell")
                                        : cellOfSlot(cap.index);
    b_.CreateStore(cell, capturedCellAddress(block, k));
  }
}

} // namespace xb
