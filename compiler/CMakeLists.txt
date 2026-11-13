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
  src/sema/Classes.cpp
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
  src/xir/passes/AppendInPlace.cpp
  src/xir/passes/FoldConstants.cpp
  src/xir/passes/Inline.cpp
  src/xir/passes/PromoteVariables.cpp
  src/xir/passes/RemoveDeadCode.cpp
  src/xir/passes/SimplifyCFG.cpp
  src/xir/passes/TypedContinuations.cpp
  src/xir/passes/Resumable.cpp
  src/xir/Printer.cpp
  src/xir/Types.cpp
  src/xir/Transform.cpp
  src/xir/Verifier.cpp
  src/codegen/CodeGen.cpp
  src/codegen/FastPaths.cpp
  src/codegen/FramePlan.cpp
  src/codegen/InlineCalls.cpp
  src/codegen/Instructions.cpp
  src/codegen/Literals.cpp
  src/codegen/LoopGuards.cpp
  src/codegen/LoopPolls.cpp
  src/codegen/LoopSplit.cpp
  src/codegen/Variables.cpp
  src/codegen/Emit.cpp
  src/driver/main.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/RuntimeSources.cpp
)

# The runtime's xBase sources whose functions calls may run in place (docs/xir.md, Inlining): compiled into the
# runtime too, and carried by xbc.
set(XB_INLINE_PRG ${PROJECT_SOURCE_DIR}/runtime/src/rtl/arrays.prg)
string(REPLACE ";" "|" XB_INLINE_LIST "${XB_INLINE_PRG}")
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/RuntimeSources.cpp
  COMMAND ${CMAKE_COMMAND} "-DSOURCES=${XB_INLINE_LIST}" -DOUT=${CMAKE_CURRENT_BINARY_DIR}/RuntimeSources.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_sources.cmake
  DEPENDS ${XB_INLINE_PRG} ${CMAKE_CURRENT_SOURCE_DIR}/embed_sources.cmake
  COMMENT "runtime sources for inlining")

# xbrt.h is included for shared constants only (type tags, ABI version).
target_include_directories(xbc PRIVATE src ${PROJECT_SOURCE_DIR}/runtime/include)
# Third-party headers must never turn into warnings of ours.
target_include_directories(xbc SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(xbc PRIVATE XB_DEFAULT_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(xbc PRIVATE XB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  XB_VERSION_MINOR=${PROJECT_VERSION_MINOR} XB_VERSION_PATCH=${PROJECT_VERSION_PATCH})
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
