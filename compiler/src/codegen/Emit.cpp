#include "codegen/Emit.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

using namespace llvm;

namespace xb {

void initializeTargets() {
  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();
}

void printTargets() { TargetRegistry::printRegisteredTargetsForVersion(outs()); }

static CodeGenOptLevel cgLevel(unsigned l) {
  switch (l) {
  case 0: return CodeGenOptLevel::None;
  case 1: return CodeGenOptLevel::Less;
  case 3: return CodeGenOptLevel::Aggressive;
  default: return CodeGenOptLevel::Default;
  }
}

std::unique_ptr<TargetMachine> createTargetMachine(const TargetConfig &cfg, std::string &err) {
  Triple triple(cfg.triple.empty() ? sys::getDefaultTargetTriple() : Triple::normalize(cfg.triple));
  const Target *target = TargetRegistry::lookupTarget(triple, err);
  if (!target) return nullptr;
  TargetOptions opts;
  std::optional<Reloc::Model> rm;
  if (cfg.pic) rm = Reloc::PIC_;
  else rm = Reloc::Static;
  std::unique_ptr<TargetMachine> tm(
      target->createTargetMachine(triple, cfg.cpu, cfg.features, opts, rm, std::nullopt, cgLevel(cfg.optLevel)));
  if (!tm) err = "cannot create target machine for '" + triple.str() + "'";
  return tm;
}

void optimizeModule(Module &m, TargetMachine &tm, unsigned optLevel) {
  LoopAnalysisManager lam;
  FunctionAnalysisManager fam;
  CGSCCAnalysisManager cgam;
  ModuleAnalysisManager mam;
  PassBuilder pb(&tm);
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  OptimizationLevel lvl = optLevel == 0 ? OptimizationLevel::O0
                        : optLevel == 1 ? OptimizationLevel::O1
                        : optLevel == 3 ? OptimizationLevel::O3
                                        : OptimizationLevel::O2;
  ModulePassManager mpm = optLevel == 0 ? pb.buildO0DefaultPipeline(lvl) : pb.buildPerModuleDefaultPipeline(lvl);
  mpm.run(m, mam);
}

bool emitObject(Module &m, TargetMachine &tm, const std::string &path, std::string &err) {
  std::error_code ec;
  raw_fd_ostream out(path, ec, sys::fs::OF_None);
  if (ec) {
    err = "cannot open '" + path + "': " + ec.message();
    return false;
  }
  legacy::PassManager pm;
  if (tm.addPassesToEmitFile(pm, out, nullptr, CodeGenFileType::ObjectFile)) {
    err = "target '" + tm.getTargetTriple().str() + "' cannot emit object files";
    return false;
  }
  pm.run(m);
  out.flush();
  return true;
}

bool dumpIR(Module &m, const std::string &path, std::string &err) {
  std::error_code ec;
  raw_fd_ostream out(path, ec, sys::fs::OF_Text);
  if (ec) {
    err = "cannot open '" + path + "': " + ec.message();
    return false;
  }
  m.print(out, nullptr);
  return true;
}

} // namespace xb
