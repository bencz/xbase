find_package(LLVM REQUIRED CONFIG)
message(STATUS "xbc: using LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")

add_executable(xbc
  src/common/Diagnostics.cpp
  src/preprocessor/Lexer.cpp
  src/preprocessor/Directives.cpp
  src/preprocessor/Preprocessor.cpp
  src/preprocessor/RuleMatch.cpp
  src/preprocessor/RuleParse.cpp
  src/preprocessor/RuleResult.cpp
  src/parser/Expressions.cpp
  src/parser/Parser.cpp
  src/parser/Statements.cpp
  src/sema/Calls.cpp
  src/sema/Sema.cpp
  src/opt/Fold.cpp
  src/xir/Analysis.cpp
  src/xir/Builder.cpp
  src/xir/Gen.cpp
  src/xir/GenExpr.cpp
  src/xir/GenStmt.cpp
  src/xir/IR.cpp
  src/xir/Ops.cpp
  src/xir/PassManager.cpp
  src/xir/passes/FoldConstants.cpp
  src/xir/passes/PromoteVariables.cpp
  src/xir/passes/RemoveDeadCode.cpp
  src/xir/passes/SimplifyCFG.cpp
  src/xir/passes/TypedContinuations.cpp
  src/xir/Printer.cpp
  src/xir/Types.cpp
  src/xir/Verifier.cpp
  src/codegen/CodeGen.cpp
  src/codegen/FastPaths.cpp
  src/codegen/FramePlan.cpp
  src/codegen/Instructions.cpp
  src/codegen/Literals.cpp
  src/codegen/LoopSplit.cpp
  src/codegen/Variables.cpp
  src/codegen/Emit.cpp
  src/driver/main.cpp
)

# xbrt.h is included for shared constants only (type tags, ABI version).
target_include_directories(xbc PRIVATE src ${PROJECT_SOURCE_DIR}/runtime/include)
# Third-party headers must never turn into warnings of ours.
target_include_directories(xbc SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(xbc PRIVATE XB_DEFAULT_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/include")
separate_arguments(XB_LLVM_DEFS NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(xbc PRIVATE ${XB_LLVM_DEFS})

if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(xbc PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-rtti>)
endif()
xb_target_warnings(xbc)

# Every target LLVM was built with is linked in: xbc can cross-compile to all of them.
if(LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(xbc PRIVATE LLVM)
else()
  llvm_map_components_to_libnames(XB_LLVM_LIBS
    core support target targetparser mc passes analysis
    ${LLVM_TARGETS_TO_BUILD})
  target_link_libraries(xbc PRIVATE ${XB_LLVM_LIBS})
endif()
