# Run tests: tests/run/<name>.prg is compiled with xbc, linked against xbrt,
# executed, and its stdout compared with <name>.out. An optional <name>.rc
# holds the expected exit code (default 0), an optional <name>.args the
# command-line arguments, one per line, an optional <name>.in its standard
# input. The modules in an optional
# directory <name>.with/ are compiled in the same xbc run (one object); those
# in an optional directory <name>.d/ are compiled on their own with --no-entry
# and linked into the program, and those in <name>.rt/ likewise as modules of
# the runtime (--runtime).
# Every program is built twice, run.<name> as xbc builds by default (-O2) and run0.<name> with -O0 (no xIR passes):
# both must give the same output, so that no optimization changes what a program does. Each test runs in a directory
# of its own, work/<test>.<name> (programs write and list files: tests running at once never see each other's).
function(xb_add_run_test_variant name suffix test workdir)
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/${name}${suffix}.o)
  file(GLOB with ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.with/*.prg)
  xb_compile_prg(${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.prg ${obj} ${with} ${ARGN})
  set(objs ${obj})
  file(GLOB modules ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.d/*.prg)
  foreach(module ${modules})
    get_filename_component(stem ${module} NAME_WE)
    set(mobj ${CMAKE_CURRENT_BINARY_DIR}/${name}${suffix}.d.${stem}.o)
    xb_compile_prg(${module} ${mobj} --no-entry ${ARGN})
    list(APPEND objs ${mobj})
  endforeach()
  file(GLOB modules ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.rt/*.prg)
  foreach(module ${modules})
    get_filename_component(stem ${module} NAME_WE)
    set(mobj ${CMAKE_CURRENT_BINARY_DIR}/${name}${suffix}.rt.${stem}.o)
    xb_compile_prg(${module} ${mobj} --runtime ${ARGN})
    list(APPEND objs ${mobj})
  endforeach()
  add_executable(t${suffix}_${name} ${objs})
  set_target_properties(t${suffix}_${name} PROPERTIES LINKER_LANGUAGE C)
  target_link_libraries(t${suffix}_${name} PRIVATE xbrt)
  set(rc 0)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.rc)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.rc rc)
    string(STRIP "${rc}" rc)
  endif()
  set(args "")
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.args)
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.args args)
  endif()
  file(MAKE_DIRECTORY ${workdir})
  add_test(NAME ${test}.${name}
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=$<TARGET_FILE:t${suffix}_${name}>;${args}"
      -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.out
      -DEXPECTED_RC=${rc}
      -DEXPECTED_ERR=${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.err
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.in
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake
    WORKING_DIRECTORY ${workdir})
  # A test that commits much memory (a recursion to the end of the stack) runs alone, never beside another.
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.serial)
    set_tests_properties(${test}.${name} PROPERTIES RUN_SERIAL TRUE)
  endif()
endfunction()

function(xb_add_run_test name)
  xb_add_run_test_variant(${name} "" run ${CMAKE_CURRENT_BINARY_DIR}/work/run.${name})
  xb_add_run_test_variant(${name} _O0 run0 ${CMAKE_CURRENT_BINARY_DIR}/work/run0.${name} -O0)
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

# The names the compiler defines from the target (tests/pp/target/defines.prg): expected in target/<triple>.ppo, or
# target/<name>.ppo for a run that also gives -D options.
function(xb_add_target_pp_test name triple)
  set(cmd $<TARGET_FILE:xbc> -E --target=${triple} ${ARGN} ${CMAKE_CURRENT_SOURCE_DIR}/pp/target/defines.prg)
  add_test(NAME pp.target_${name}
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=${cmd}"
      -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/pp/target/${name}.ppo
      -DEXPECTED_RC=0
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
endfunction()
foreach(triple powerpc-linux-gnu s390x-ibm-linux-gnu x86_64-pc-windows-msvc aarch64-apple-darwin
               armv7-unknown-freebsd riscv64-unknown-linux-gnu)
  xb_add_target_pp_test(${triple} ${triple})
endforeach()
xb_add_target_pp_test(command_line i686-pc-linux-gnu -D__ARCH32BIT__=7 -D__XBASE_CPU__="x86")

# Compile-failure tests: compiling tests/fail/<name>.prg (with the modules of
# an optional directory <name>.with/, and the xbc options of an optional
# <name>.args, one per line) must fail with the message in <name>.err.
file(GLOB XB_FAIL_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/fail ${CMAKE_CURRENT_SOURCE_DIR}/fail/*.prg)
foreach(t ${XB_FAIL_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  file(GLOB with ${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.with/*.prg)
  set(options "")
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.args)
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.args options)
  endif()
  set(cmd $<TARGET_FILE:xbc> ${options} ${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.prg ${with}
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

# A loop without calls polls the pending requests on its back edge (an atomic load marked xb.poll; SUM's entry is the
# function of the symbol: its direct one, called by nothing else once MAIN inlines SUM, is inlined into it).
xb_add_codegen_test(loop_back_edge_poll x86_64-linux-gnu
  "@xbp_SUM\\(.*load atomic i32, ptr %[0-9a-z]+ monotonic[^\n]*!xb\\.poll" loop_poll)
# Every function tests its frame against the stack limit before its body.
xb_add_codegen_test(stack_limit_check x86_64-linux-gnu "@xbp_SUM\\(.*call void @xb_stack_overflow" loop_poll)
# A counted loop polls when it reaches a stop, in its exit test's branch; a loop whose end depends on its data counts
# its passes down between polls (LoopPolls).
xb_add_codegen_test(loop_strip_mined x86_64-linux-gnu "\npoll\\.reached[0-9]*:" loop_stops)
xb_add_codegen_test(loop_count_down x86_64-linux-gnu "%poll\\.left[0-9.a-z]* = phi i32" loop_stops)
# A loop's generic copy goes back on its back edge to a second fast copy of the loop, through a preheader of its own.
xb_add_codegen_test(loop_reenter x86_64-linux-gnu "\n[A-Za-z0-9._]*\\.reenter:" loop_reenter)

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

# Platform tests: C programs that exercise the runtime's own services (threads, locks, atomics, waiting) directly,
# linked with the runtime (and with platform/<name>.prg, compiled with --no-entry, when the test calls xBase code);
# each one exits with 0 when every check passed.
file(GLOB XB_PLATFORM_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/platform ${CMAKE_CURRENT_SOURCE_DIR}/platform/*.c)
foreach(t ${XB_PLATFORM_TESTS})
  get_filename_component(name ${t} NAME_WE)
  set(prg)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/platform/${name}.prg) # xBase functions the test calls
    set(prg ${CMAKE_CURRENT_BINARY_DIR}/p_${name}.prg.o)
    xb_compile_prg(${CMAKE_CURRENT_SOURCE_DIR}/platform/${name}.prg ${prg} --no-entry)
  endif()
  add_executable(p_${name} platform/${t} ${prg})
  xb_target_warnings(p_${name})
  if(MSVC)
    target_compile_options(p_${name} PRIVATE /volatile:ms)
  endif()
  target_include_directories(p_${name} PRIVATE ${PROJECT_SOURCE_DIR}/runtime/src ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(p_${name} PRIVATE xbrt)
  add_test(NAME platform.${name} COMMAND p_${name})
endforeach()

# A test that hangs or loops must fail quickly instead of exhausting the machine.
get_property(XB_ALL_TESTS DIRECTORY PROPERTY TESTS)
set_tests_properties(${XB_ALL_TESTS} PROPERTIES TIMEOUT 30)
