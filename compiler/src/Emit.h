#pragma once

#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>
#include <string>

namespace xb {

struct TargetConfig {
  std::string triple;     // empty = host
  std::string cpu = "generic";
  std::string features;
  unsigned optLevel = 2;  // 0..3
  bool pic = true;
};

void initializeTargets();
void printTargets();
std::unique_ptr<llvm::TargetMachine> createTargetMachine(const TargetConfig &cfg, std::string &err);
void optimizeModule(llvm::Module &m, llvm::TargetMachine &tm, unsigned optLevel);
// Writes a native object file straight from the in-memory module.
bool emitObject(llvm::Module &m, llvm::TargetMachine &tm, const std::string &path, std::string &err);
// Debug aid only: textual dump of the module. Not part of the pipeline.
bool dumpIR(llvm::Module &m, const std::string &path, std::string &err);

} // namespace xb
