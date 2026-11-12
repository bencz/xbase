#include "codegen/Emit.h"

#include "codegen/LoopPolls.h"
#include "codegen/LoopSplit.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
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

// The CPU and features: as given, or with `native` the machine xbc runs on
// (its CPU and every feature it has, then the explicit features). `native`
// only makes sense for the host target: code built for it may not run
// elsewhere, which is why the default is `generic`.
static bool resolveCpu(const TargetConfig &cfg, const Triple &triple, std::string &cpu, std::string &features,
                       std::string &err) {
  cpu = cfg.cpu;
  features = cfg.features;
  if (cfg.cpu != "native") return true;
  if (triple != Triple(sys::getDefaultTargetTriple())) {
    err = "--cpu=native needs the host target (" + sys::getDefaultTargetTriple() + ")";
    return false;
  }
  cpu = sys::getHostCPUName().str();
  std::string host;
  for (const auto &f : sys::getHostCPUFeatures()) {
    if (!host.empty()) host += ",";
    host += (f.second ? "+" : "-") + f.first().str();
  }
  features = cfg.features.empty() ? host : host + "," + cfg.features;
  return true;
}

Triple targetTriple(const TargetConfig &cfg) {
  return Triple(cfg.triple.empty() ? sys::getDefaultTargetTriple() : Triple::normalize(cfg.triple));
}

std::unique_ptr<TargetMachine> createTargetMachine(const TargetConfig &cfg, std::string &err) {
  Triple triple = targetTriple(cfg);
  const Target *target = TargetRegistry::lookupTarget(triple, err);
  if (!target) return nullptr;
  std::string cpu, features;
  if (!resolveCpu(cfg, triple, cpu, features, err)) return nullptr;
  TargetOptions opts;
  std::optional<Reloc::Model> rm;
  if (cfg.pic) rm = Reloc::PIC_;
  else rm = Reloc::Static;
  std::unique_ptr<TargetMachine> tm(
      target->createTargetMachine(triple, cpu, features, opts, rm, std::nullopt, cgLevel(cfg.optLevel)));
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
  if (optLevel == 0) return;
  // Loops whose slow paths leave for a generic copy (LoopSplit.h), then fewer polls on the back edges of the fast
  // loops that leaves (LoopPolls.h); after each, the function simplification pipeline again on the functions it
  // changed.
  FunctionPassManager fpm = pb.buildFunctionSimplificationPipeline(lvl, ThinOrFullLTOPhase::None);
  auto again = [&](bool (*change)(Function &, FunctionAnalysisManager &), const char *what) {
    std::vector<Function *> changed;
    for (Function &f : m)
      if (!f.isDeclaration() && change(f, fam)) {
        changed.push_back(&f);
        fam.invalidate(f, PreservedAnalyses::none());
      }
    if (changed.empty()) return;
    std::string err;
    raw_string_ostream os(err);
    if (verifyModule(m, &os)) report_fatal_error(Twine("xbc: internal error: ") + what + " broke the module:\n" + err);
    for (Function *f : changed) fpm.run(*f, fam);
  };
  again([](Function &f, FunctionAnalysisManager &) { return splitColdLoopPaths(f); }, "loop splitting");
  again(placeLoopPolls, "loop polls");
}

// Code generation runs on the legacy pass manager: in LLVM 22 it is the one
// every target implements (TargetMachine::buildCodeGenPipeline, the new pass
// manager's code generation, is only provided by a few targets, experimentally).
// The optimization pipeline above uses the new pass manager.
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
