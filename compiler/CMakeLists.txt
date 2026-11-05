find_package(LLVM REQUIRED CONFIG)
message(STATUS "xbc: using LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")

add_executable(xbc
  src/main.cpp
  src/Diagnostics.cpp
  src/Lexer.cpp
  src/Preprocessor.cpp
  src/Parser.cpp
  src/Sema.cpp
  src/CodeGen.cpp
  src/Emit.cpp
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
