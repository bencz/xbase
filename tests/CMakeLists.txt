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

# Warning tests: compiling tests/warn/<name>.prg at -O2 must succeed with the warning in <name>.warn (none when it is
# empty).
file(GLOB XB_WARN_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/warn ${CMAKE_CURRENT_SOURCE_DIR}/warn/*.prg)
foreach(t ${XB_WARN_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  set(cmd $<TARGET_FILE:xbc> -O2 ${CMAKE_CURRENT_SOURCE_DIR}/warn/${t}.prg -o ${CMAKE_CURRENT_BINARY_DIR}/warn_${t}.o)
  add_test(NAME warn.${t}
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=${cmd}"
      -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/warn/${t}.warn
      -P ${CMAKE_CURRENT_SOURCE_DIR}/warn_test.cmake)
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

# Code generation checks on cross targets: the IR xbc emits for <triple> must match a regular expression
# (xb_add_codegen_test), or must not (xb_add_codegen_absence_test; tests/codegen_test.cmake). The source is
# tests/run/funcs.prg, or the run test named by an optional fourth argument (a path from tests/ when it holds a /,
# without .prg).
function(xb_codegen_test name triple regex absent)
  set(source run/funcs)
  if(ARGC GREATER 4)
    set(source ${ARGV4})
    if(NOT source MATCHES "/")
      set(source run/${source})
    endif()
  endif()
  add_test(NAME codegen.${name}
    COMMAND ${CMAKE_COMMAND}
      -DXBC=$<TARGET_FILE:xbc>
      -DNAME=${name}
      -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source}.prg
      -DTRIPLE=${triple}
      -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
      "-DREGEX=${regex}"
      -DABSENT=${absent}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/codegen_test.cmake)
endfunction()
function(xb_add_codegen_test name triple regex)
  xb_codegen_test(${name} ${triple} "${regex}" OFF ${ARGN})
endfunction()
function(xb_add_codegen_absence_test name triple regex)
  xb_codegen_test(${name} ${triple} "${regex}" ON ${ARGN})
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
# Calls, function references and REQUESTs of an xbase library's functions reach its symbols, on a 32-bit and a
# big-endian target.
xb_add_codegen_test(library_call_i686 i686-pc-linux-gnu "call void @\"xbx_GREET\\$TWICE\"" codegen/library_call)
xb_add_codegen_test(library_call_ppc64 powerpc64-unknown-linux-gnu
                    "@xb.module.requests = private constant \\[1 x ptr\\] \\[ptr @\"xbx_GREET\\$DECORATE\"\\]"
                    codegen/library_call)
# A program that runs bodies of the runtime's carried sources requires the stamps of their texts, which only a runtime
# built from the same texts defines (a mismatch fails to link).
set(stamps "@xb.source.stamps = private constant \\[2 x ptr\\] ")
string(APPEND stamps "\\[ptr @\"xbl_ARRAYS\\$source\\$[0-9a-f]+\", ptr @\"xbl_COLLECTIONS\\$source\\$[0-9a-f]+\"\\]")
xb_add_codegen_test(carried_source_stamps powerpc64-unknown-linux-gnu "${stamps}" carried_methods)
xb_add_codegen_test(s390x_string_hash s390x-linux-gnu "i32 239963683, i64 7, \\[8 x i8\\] c\"cliente" hashes)
xb_add_codegen_test(ppc32_string_hash powerpc-linux-gnu "i32 239963683, i32 7, \\[8 x i8\\] c\"cliente" hashes)
# An item copy reads the value word by an unordered atomic load where 64 bits are a native integer (big-endian ppc64
# too) and by a plain one on 32-bit targets, where an atomic one calls the atomics library (on x86, whose items are
# 4-byte aligned, a call that locks a mutex).
xb_add_codegen_test(item_word_x86_64 x86_64-linux-gnu "= load atomic i64, ptr [^\n]* unordered, align 8")
xb_add_codegen_test(item_word_ppc64 powerpc64-unknown-linux-gnu "= load atomic i64, ptr [^\n]* unordered, align 8")
xb_add_codegen_absence_test(item_word_ppc32 powerpc-linux-gnu "load atomic (i64|double)")
xb_add_codegen_absence_test(item_word_i686 i686-pc-linux-gnu "load atomic (i64|double)")

# A loop without calls polls the pending requests on its back edge (an atomic load marked xb.poll; SUM's entry is the
# function of the symbol: its direct one, called by nothing else once MAIN inlines SUM, is inlined into it).
xb_add_codegen_test(loop_back_edge_poll x86_64-linux-gnu
  "@xbp_SUM\\(.*load atomic i32, ptr %[0-9a-z]+ monotonic[^\n]*!xb\\.poll" loop_poll)
# Every function tests its frame against the stack limit before its body.
xb_add_codegen_test(stack_limit_check x86_64-linux-gnu "@xbp_SUM\\(.*call void @xb_stack_overflow" loop_poll)
# Below it, xb_stack_deep first (a deep stack's mark), declared to touch the context alone (the body after it stays
# optimized as without it).
xb_add_codegen_test(stack_deep_mark x86_64-linux-gnu
  "memory\\(argmem: readwrite\\)\ndeclare i32 @xb_stack_deep\\(ptr, i64\\).*@xbp_SUM\\(.*call i32 @xb_stack_deep"
  loop_poll)
# A counted loop polls when it reaches a stop, in its exit test's branch; a loop whose end depends on its data counts
# its passes down between polls (LoopPolls).
xb_add_codegen_test(loop_strip_mined x86_64-linux-gnu "\npoll\\.reached[0-9]*:" loop_stops)
xb_add_codegen_test(loop_count_down x86_64-linux-gnu "%poll\\.left[0-9.a-z]* = phi i32" loop_stops)
# A loop's generic copy goes back on its back edge to a second fast copy of the loop, through a preheader of its own.
xb_add_codegen_test(loop_reenter x86_64-linux-gnu "\n[A-Za-z0-9._]*\\.reenter:" loop_reenter)
# A nest whose fast copy makes no call is split whole: its outer loop is entered again too (docs/compiler.md, object
# emission).
# The generic copy's back edge enters the loop again: through a preheader of its own, or an edge block LLVM made of it.
xb_add_codegen_test(loop_nest_split x86_64-linux-gnu
  "\n(for\\.test(\\.i)?\\.reenter|[A-Za-z0-9._]*\\.generic\\.for\\.test(\\.i)?\\.again[A-Za-z0-9._]*):" loop_nest_split)
# A loop that makes no call reads on entry the sharing state of a matrix it indexes by its counter.
xb_add_codegen_test(loop_entry_state x86_64-linux-gnu "\n[A-Za-z0-9._]*\\.states[0-9]*:" loop_nests)

# xIR tests: the xIR of tests/xir/<name>.prg (--dump-xir) must equal <name>.xir; types_<name> with the inferred types
# (--dump-xir-types).
file(GLOB XB_XIR_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/xir ${CMAKE_CURRENT_SOURCE_DIR}/xir/*.prg)
foreach(t ${XB_XIR_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  string(REGEX MATCH "^types_" types ${t})
  add_test(NAME xir.${t}
    COMMAND ${CMAKE_COMMAND} -DXBC=$<TARGET_FILE:xbc> -DTYPES=${types} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/xir/${t}.prg
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

# The install (cmake --install into a folder of the build) and a program built from it alone (tests/package): the
# layout, and xbc finding the standard headers there by itself. A build for another target (XB_TARGET_C_FLAGS) builds
# the program for it too.
set(XB_PACKAGE ${CMAKE_CURRENT_BINARY_DIR}/package)
list(JOIN XB_TARGET_C_FLAGS " " XB_PACKAGE_C_FLAGS)
add_test(NAME package.install
  COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${XB_PACKAGE}/root --config $<CONFIG>)
add_test(NAME package.program
  COMMAND ${CMAKE_CTEST_COMMAND} --build-and-test ${CMAKE_CURRENT_SOURCE_DIR}/package ${XB_PACKAGE}/build
          --build-generator ${CMAKE_GENERATOR} --build-makeprogram ${CMAKE_MAKE_PROGRAM} --build-config $<CONFIG>
          --build-options -DXBASE_ROOT=${XB_PACKAGE}/root -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                          "-DCMAKE_C_FLAGS=${XB_PACKAGE_C_FLAGS}" -DXBC_TARGET=${XB_TARGET}
          --test-command ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure)
set_tests_properties(package.install PROPERTIES FIXTURES_SETUP package)
set_tests_properties(package.program PROPERTIES FIXTURES_REQUIRED package)

# Programs linked by xbc (docs/projects.md): not with the MSVC tools, whose link.exe only a developer prompt finds.
if(NOT MSVC)

# The libraries' own tests (libs/<library>/tests/<name>.prg, expecting <name>.out), as programs xbc compiles and links
# with their library (--lib, --link), at -O0 and -O2.
file(GLOB XB_LIBRARY_TESTS ${PROJECT_SOURCE_DIR}/libs/*/tests/*.prg)
foreach(src ${XB_LIBRARY_TESTS})
  get_filename_component(name ${src} NAME_WE)
  get_filename_component(dir ${src} DIRECTORY)
  get_filename_component(library ${dir} DIRECTORY)
  get_filename_component(library ${library} NAME)
  foreach(level 0 2)
    set(exe ${CMAKE_CURRENT_BINARY_DIR}/libs/${library}/${name}_O${level}${CMAKE_EXECUTABLE_SUFFIX})
    xb_build_project(OUTPUTS ${exe} DEPENDS xblib_${library}
                     ARGS ${src} --lib=${library} --link -O${level} -o ${exe} COMMENT "xbc ${name}.prg --link")
    add_custom_target(t_lib_${library}_${name}_O${level} ALL DEPENDS ${exe})
    add_test(NAME lib${level}.${library}.${name}
      COMMAND ${CMAKE_COMMAND} -DEXE=${exe} -DEXPECTED=${dir}/${name}.out -DEXPECTED_RC=0
              -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
  endforeach()
endforeach()

# Projects (tests/projects/<name>.xbprj, with the xbc options of an optional <name>.args, one a line, @TARGET@ standing
# for --target=XB_TARGET when the build names one): one with <name>.out is built and run (its output and exit code 0),
# with the test libraries greet (tests/projects/greet, which uses cli) and other (which declares a function greet
# declares too) beside it; one with <name>.err must fail to build with that message.
set(XB_TEST_PROJECTS ${CMAKE_CURRENT_BINARY_DIR}/projects)
set(XB_TEST_LIBRARIES "")
foreach(library greet other)
  set(archive ${XB_TEST_PROJECTS}/${library}/${CMAKE_STATIC_LIBRARY_PREFIX}${library}${CMAKE_STATIC_LIBRARY_SUFFIX})
  xb_build_project(OUTPUTS ${archive} ${XB_TEST_PROJECTS}/${library}/${library}.xbm DEPENDS xblib_cli
                   ARGS ${CMAKE_CURRENT_SOURCE_DIR}/projects/${library}/${library}.xbprj -o ${archive}
                   COMMENT "xbc ${library}.xbprj")
  add_custom_target(t_project_${library} ALL DEPENDS ${archive})
  list(APPEND XB_TEST_LIBRARIES t_project_${library} ${archive})
endforeach()
file(GLOB XB_PROJECT_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/projects/*.xbprj)
foreach(prj ${XB_PROJECT_TESTS})
  get_filename_component(name ${prj} NAME_WE)
  set(base ${CMAKE_CURRENT_SOURCE_DIR}/projects/${name})
  set(options "")
  if(EXISTS ${base}.args)
    file(STRINGS ${base}.args options)
  endif()
  # @TARGET@: the build's target, for a project that uses the libraries the build compiled for it.
  set(target "")
  if(XB_TARGET)
    set(target --target=${XB_TARGET})
  endif()
  list(TRANSFORM options REPLACE "^@TARGET@$" "${target}")
  list(REMOVE_ITEM options "")
  if(EXISTS ${base}.out)
    set(exe ${XB_TEST_PROJECTS}/${name}${CMAKE_EXECUTABLE_SUFFIX})
    xb_build_project(OUTPUTS ${exe} DEPENDS ${XB_TEST_LIBRARIES}
                     ARGS ${prj} --out-dir=${XB_TEST_PROJECTS} -L${XB_TEST_PROJECTS} ${options}
                     COMMENT "xbc ${name}.xbprj")
    add_custom_target(t_project_${name} ALL DEPENDS ${exe})
    add_test(NAME project.${name}
      COMMAND ${CMAKE_COMMAND} -DEXE=${exe} -DEXPECTED=${base}.out -DEXPECTED_RC=0
              -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
  else()
    set(cmd $<TARGET_FILE:xbc> ${prj} --out-dir=${XB_TEST_PROJECTS}/fail -L${XB_TEST_PROJECTS}
        -L${PROJECT_BINARY_DIR}/libs ${options})
    add_test(NAME project.${name}
      COMMAND ${CMAKE_COMMAND} "-DEXE=${cmd}"
        -DEXPECTED=${base}.err -P ${CMAKE_CURRENT_SOURCE_DIR}/fail_test.cmake)
  endif()
endforeach()
# The dependency file of a program with libraries names, besides its sources, its project file, the libraries'
# module files and archives and the runtime's, so that a build system rebuilds it when one changes.
set(names uses_greet.prg uses_greet.xbprj greet.xbm ${CMAKE_STATIC_LIBRARY_PREFIX}greet${CMAKE_STATIC_LIBRARY_SUFFIX}
          cli.xbm ${XB_RUNTIME_MODULE}.xbm $<TARGET_FILE_NAME:xbrt>)
add_test(NAME project.depfile
  COMMAND ${CMAKE_COMMAND} -DFILE=${XB_TEST_PROJECTS}/uses_greet${CMAKE_EXECUTABLE_SUFFIX}.d "-DNAMES=${names}"
          -P ${CMAKE_CURRENT_SOURCE_DIR}/depfile_test.cmake)
# A project's settings for another platform's target (--target, preprocessing only).
add_test(NAME project.qualified_windows
  COMMAND ${CMAKE_COMMAND}
    "-DEXE=$<TARGET_FILE:xbc>;${CMAKE_CURRENT_SOURCE_DIR}/projects/qualified.xbprj;--target=x86_64-pc-windows-msvc;-E"
    "-DEXPECTED=QOut( \"windows\" == \"windows\" )" -P ${CMAKE_CURRENT_SOURCE_DIR}/contains_test.cmake)
# The libraries' modules registered before the program's (library_order_test.cmake).
add_test(NAME project.library_order
  COMMAND ${CMAKE_COMMAND} -DXBC=$<TARGET_FILE:xbc> -DAR=${CMAKE_AR}
          "-DLIBDIRS=${XB_TEST_PROJECTS};${PROJECT_BINARY_DIR}/libs"
          -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/projects/uses_greet.prg
          -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/projects/uses_greet.out -DTARGET=${XB_TARGET}
          -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/library_order_test.cmake)
# A library's stamp (library_stamp_test.cmake).
add_test(NAME project.library_stamp
  COMMAND ${CMAKE_COMMAND} -DXBC=$<TARGET_FILE:xbc> -DTARGET=${XB_TARGET} -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/library_stamp_test.cmake)

# The samples (samples/<sample>/<sample>.xbprj), built by xbc from their project files. Each tests/samples/<case>.out
# runs the sample its name starts with (<sample>.out, <sample>.<case>.out) from the sample's folder, with the arguments
# of <case>.args (one a line) and the exit code of <case>.rc (else 0).
file(GLOB XB_SAMPLES LIST_DIRECTORIES true ${PROJECT_SOURCE_DIR}/samples/*)
foreach(dir ${XB_SAMPLES})
  get_filename_component(sample ${dir} NAME)
  set(exe ${CMAKE_CURRENT_BINARY_DIR}/samples/${sample}${CMAKE_EXECUTABLE_SUFFIX})
  xb_build_project(OUTPUTS ${exe} DEPENDS xblib_cli
                   ARGS ${dir}/${sample}.xbprj --out-dir=${CMAKE_CURRENT_BINARY_DIR}/samples
                   COMMENT "xbc ${sample}.xbprj")
  add_custom_target(t_sample_${sample} ALL DEPENDS ${exe})
endforeach()
file(GLOB XB_SAMPLE_CASES ${CMAKE_CURRENT_SOURCE_DIR}/samples/*.out)
foreach(out ${XB_SAMPLE_CASES})
  get_filename_component(sample ${out} NAME_WE)
  get_filename_component(case ${out} NAME)
  string(REGEX REPLACE "\\.out$" "" case ${case})
  set(args "")
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples/${case}.args)
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/samples/${case}.args args)
  endif()
  set(rc 0)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples/${case}.rc)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/samples/${case}.rc rc)
    string(STRIP "${rc}" rc)
  endif()
  add_test(NAME sample.${case}
    COMMAND ${CMAKE_COMMAND} "-DEXE=${CMAKE_CURRENT_BINARY_DIR}/samples/${sample}${CMAKE_EXECUTABLE_SUFFIX};${args}"
            -DEXPECTED=${out} -DEXPECTED_RC=${rc} -DEXPECTED_ERR=${CMAKE_CURRENT_SOURCE_DIR}/samples/${case}.err
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/samples/${sample})
endforeach()

endif()

# A test that hangs or loops must fail quickly instead of exhausting the machine.
get_property(XB_ALL_TESTS DIRECTORY PROPERTY TESTS)
set_tests_properties(${XB_ALL_TESTS} PROPERTIES TIMEOUT 30)
set_tests_properties(package.program PROPERTIES TIMEOUT 300)
