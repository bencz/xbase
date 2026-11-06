# Run tests: tests/run/<name>.prg is compiled with xbc, linked against xbrt,
# executed, and its stdout compared with <name>.out. An optional <name>.rc
# holds the expected exit code (default 0), an optional <name>.args the
# command-line arguments, one per line. The modules in an optional
# directory <name>.with/ are compiled in the same xbc run (one object); those
# in an optional directory <name>.d/ are compiled on their own with --no-entry
# and linked into the program.
function(xb_add_run_test name)
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/${name}.o)
  file(GLOB with ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.with/*.prg)
  xb_compile_prg(${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.prg ${obj} ${with})
  set(objs ${obj})
  file(GLOB modules ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.d/*.prg)
  foreach(module ${modules})
    get_filename_component(stem ${module} NAME_WE)
    set(mobj ${CMAKE_CURRENT_BINARY_DIR}/${name}.d.${stem}.o)
    xb_compile_prg(${module} ${mobj} --no-entry)
    list(APPEND objs ${mobj})
  endforeach()
  add_executable(t_${name} ${objs})
  set_target_properties(t_${name} PROPERTIES LINKER_LANGUAGE C)
  target_link_libraries(t_${name} PRIVATE xbrt)
  set(rc 0)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.rc)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.rc rc)
    string(STRIP "${rc}" rc)
  endif()
  set(args "")
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.args)
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.args args)
  endif()
  add_test(NAME run.${name}
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=$<TARGET_FILE:t_${name}>;${args}"
      -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.out
      -DEXPECTED_RC=${rc}
      -DEXPECTED_ERR=${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.err
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
endfunction()

file(GLOB XB_RUN_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/run ${CMAKE_CURRENT_SOURCE_DIR}/run/*.prg)
foreach(t ${XB_RUN_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  xb_add_run_test(${t})
endforeach()

# Preprocessor tests: `xbc -E tests/pp/<name>.prg` must produce <name>.ppo.
file(GLOB XB_PP_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/pp ${CMAKE_CURRENT_SOURCE_DIR}/pp/*.prg)
foreach(t ${XB_PP_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  add_test(NAME pp.${t}
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=$<TARGET_FILE:xbc>;-E;${CMAKE_CURRENT_SOURCE_DIR}/pp/${t}.prg"
      -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/pp/${t}.ppo
      -DEXPECTED_RC=0
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
endforeach()

# Compile-failure tests: compiling tests/fail/<name>.prg (with the modules of
# an optional directory <name>.with/) must fail with the message in <name>.err.
file(GLOB XB_FAIL_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/fail ${CMAKE_CURRENT_SOURCE_DIR}/fail/*.prg)
foreach(t ${XB_FAIL_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  file(GLOB with ${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.with/*.prg)
  set(cmd $<TARGET_FILE:xbc> ${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.prg ${with}
      -o ${CMAKE_CURRENT_BINARY_DIR}/fail_${t}.o)
  add_test(NAME fail.${t}
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=${cmd}"
      -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.err
      -P ${CMAKE_CURRENT_SOURCE_DIR}/fail_test.cmake)
endforeach()

# -u<file> names a file relative to the current directory.
add_test(NAME pp.custom_std
  COMMAND ${CMAKE_COMMAND}
    "-DEXE=$<TARGET_FILE:xbc>;-umystd.ch;-E;use_custom_std.prg"
    -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/pp/custom_std/use_custom_std.ppo
    -DEXPECTED_RC=0
    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/pp/custom_std)

# Several sources and a file pattern xbc expands itself (CTest passes `*` as
# it is): sorted, a file starting with a dot left out, b.prg compiled once.
add_test(NAME pp.sources_pattern
  COMMAND ${CMAKE_COMMAND}
    "-DEXE=$<TARGET_FILE:xbc>;-E;glob/b.prg;glob/*.prg"
    -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/pp/glob/expected.ppo
    -DEXPECTED_RC=0
    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/pp)

# Code generation checks on cross targets: the IR xbc emits for <triple> must
# match a regular expression (tests/codegen_test.cmake). The source is
# tests/run/funcs.prg, or the run test named by an optional fourth argument.
function(xb_add_codegen_test name triple regex)
  set(source funcs)
  if(ARGC GREATER 3)
    set(source ${ARGV3})
  endif()
  add_test(NAME codegen.${name}
    COMMAND ${CMAKE_COMMAND}
      -DXBC=$<TARGET_FILE:xbc>
      -DNAME=${name}
      -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/run/${source}.prg
      -DTRIPLE=${triple}
      -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
      "-DREGEX=${regex}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/codegen_test.cmake)
endfunction()

# 32-bit runtime arguments are extended as the target ABI requires.
xb_add_codegen_test(s390x_i32_zeroext s390x-linux-gnu "@xb_abi_check\\(i32 zeroext, i32 zeroext,")
xb_add_codegen_test(riscv64_i32_signext riscv64-linux-gnu "@xb_abi_check\\(i32 signext, i32 signext,")
xb_add_codegen_test(x86_64_i32_plain x86_64-linux-gnu "@xb_abi_check\\(i32, i32,")
# Accesses carry their memory class (TBAA): an element store, and the class's
# type node.
xb_add_codegen_test(tbaa_heap_items x86_64-linux-gnu "store [^\n]*!tbaa .*!\"heap item\"" aliasing)
# String literals carry the hash of their bytes, the same value on every
# target (the runtime computes it alike for other strings), in the header laid
# out for the target.
xb_add_codegen_test(x86_64_string_hash x86_64-linux-gnu "i32 239963683, i64 7, \\[8 x i8\\] c\"cliente" hashes)
xb_add_codegen_test(s390x_string_hash s390x-linux-gnu "i32 239963683, i64 7, \\[8 x i8\\] c\"cliente" hashes)
xb_add_codegen_test(ppc32_string_hash powerpc-linux-gnu "i32 239963683, i32 7, \\[8 x i8\\] c\"cliente" hashes)

# xIR tests: the xIR of tests/xir/<name>.prg (--dump-xir) must equal <name>.xir.
file(GLOB XB_XIR_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/xir ${CMAKE_CURRENT_SOURCE_DIR}/xir/*.prg)
foreach(t ${XB_XIR_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  add_test(NAME xir.${t}
    COMMAND ${CMAKE_COMMAND} -DXBC=$<TARGET_FILE:xbc> -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/xir/${t}.prg
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/xir/${t}.xir -DOUT=${CMAKE_CURRENT_BINARY_DIR}/xir_${t}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/xir_test.cmake)
endforeach()

# include/rtl.ch declares exactly the functions the runtime defines.
add_test(NAME runtime.declarations
  COMMAND ${CMAKE_COMMAND} -DRTL=${PROJECT_SOURCE_DIR}/include/rtl.ch -DRUNTIME_SRC=${PROJECT_SOURCE_DIR}/runtime/src
          -P ${CMAKE_CURRENT_SOURCE_DIR}/declarations_test.cmake)

# A test that hangs or loops must fail quickly instead of exhausting the machine.
get_property(XB_ALL_TESTS DIRECTORY PROPERTY TESTS)
set_tests_properties(${XB_ALL_TESTS} PROPERTIES TIMEOUT 30)
