# Every target LLVM was built with is linked in: xbc can cross-compile to all of them. LLVM's shared library when it
# was built with one, unless XB_LLVM_STATIC asks for its static libraries (a package's xbc that needs no LLVM DLL),
# and then the libraries LLVM's own need statically too where the system has them: zlib (FindZLIB's static choice,
# made before LLVM looks for it) and zstd (its shared target's import library replaced by the static one beside it).
option(XB_LLVM_STATIC "link xbc with LLVM's static libraries even where LLVM has a shared one" OFF)
if(XB_LLVM_STATIC)
  find_library(XB_ZLIB_ARCHIVE NAMES libz.a zlibstatic)
  if(XB_ZLIB_ARCHIVE)
    set(ZLIB_USE_STATIC_LIBS ON)
  endif()
endif()
find_package(LLVM REQUIRED CONFIG)
message(STATUS "xbc: using LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")
if(XB_LLVM_STATIC AND TARGET zstd::libzstd_shared)
  foreach(property IMPORTED_IMPLIB IMPORTED_IMPLIB_RELEASE IMPORTED_IMPLIB_NOCONFIG)
    get_target_property(implib zstd::libzstd_shared ${property})
    if(implib MATCHES "\\.dll\\.a$")
      string(REGEX REPLACE "\\.dll\\.a$" ".a" archive "${implib}")
      if(EXISTS "${archive}")
        set_target_properties(zstd::libzstd_shared PROPERTIES ${property} "${archive}")
      endif()
    endif()
  endforeach()
endif()

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
  src/xir/passes/PeelTypes.cpp
  src/xir/passes/VersionEach.cpp
  src/xir/passes/Specialize.cpp
  src/xir/passes/ResolveSends.cpp
  src/xir/ClassModel.cpp
  ${PROJECT_SOURCE_DIR}/runtime/src/types/members.c
  src/xir/passes/Resumable.cpp
  src/xir/passes/Versioning.cpp
  src/xir/Printer.cpp
  src/xir/Types.cpp
  src/xir/Transform.cpp
  src/xir/Verifier.cpp
  src/codegen/CodeGen.cpp
  src/codegen/FastPaths.cpp
  src/codegen/FramePlan.cpp
  src/codegen/FrameSetup.cpp
  src/codegen/InlineCalls.cpp
  src/codegen/Instructions.cpp
  src/codegen/Literals.cpp
  src/codegen/LoopGuards.cpp
  src/codegen/LoopPolls.cpp
  src/codegen/LoopSplit.cpp
  src/codegen/MemoryTags.cpp
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
# The standard headers are looked for where the install puts them (XB_INCLUDE_FROM_BIN, top-level CMakeLists.txt)
# and, when XB_SOURCE_INCLUDE is on, in this source tree (xbc run from its build folder, as the tests run it); a
# package is built without it, so its xbc holds no path of the machine that built it.
option(XB_SOURCE_INCLUDE "xbc also looks for the standard headers in this source tree (the tests need it)" ON)
target_compile_definitions(xbc PRIVATE XB_INCLUDE_FROM_BIN="${XB_INCLUDE_FROM_BIN}")
if(XB_SOURCE_INCLUDE)
  target_compile_definitions(xbc PRIVATE XB_DEFAULT_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/include")
endif()
target_compile_definitions(xbc PRIVATE XB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  XB_VERSION_MINOR=${PROJECT_VERSION_MINOR} XB_VERSION_PATCH=${PROJECT_VERSION_PATCH})
separate_arguments(XB_LLVM_DEFS NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(xbc PRIVATE ${XB_LLVM_DEFS})

if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(xbc PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-rtti>)
endif()
xb_target_warnings(xbc)
# xbc folds ^ with xbmath.h: on a 32-bit x86 host, with SSE2 as the runtime.
set(XB_HOST_X86_32 OFF)
if(CMAKE_CXX_SIZEOF_DATA_PTR EQUAL 4 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86|AMD64|x86_64)$")
  set(XB_HOST_X86_32 ON)
endif()
xb_exact_doubles(xbc ${XB_HOST_X86_32})

# LLVM's libraries (XB_LLVM_STATIC, above).
if(LLVM_LINK_LLVM_DYLIB AND NOT XB_LLVM_STATIC)
  target_link_libraries(xbc PRIVATE LLVM)
else()
  llvm_map_components_to_libnames(XB_LLVM_LIBS
    core support target targetparser mc passes analysis
    ${LLVM_TARGETS_TO_BUILD})
  target_link_libraries(xbc PRIVATE ${XB_LLVM_LIBS})
endif()

install(TARGETS xbc RUNTIME DESTINATION ${XB_INSTALL_BIN})
