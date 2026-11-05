# Run tests: tests/run/<name>.prg is compiled with xbc, linked against xbrt,
# executed, and its stdout compared with <name>.out. An optional <name>.rc
# holds the expected exit code (default 0).
function(xb_add_run_test name)
  set(src ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.prg)
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/${name}.o)
  add_custom_command(
    OUTPUT ${obj}
    COMMAND xbc ${src} -o ${obj} --depfile=${obj}.d
    DEPENDS xbc ${src}
    DEPFILE ${obj}.d
    COMMENT "xbc ${name}.prg")
  set_source_files_properties(${obj} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  add_executable(t_${name} ${obj})
  set_target_properties(t_${name} PROPERTIES LINKER_LANGUAGE C)
  target_link_libraries(t_${name} PRIVATE xbrt)
  set(rc 0)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.rc)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/run/${name}.rc rc)
    string(STRIP "${rc}" rc)
  endif()
  add_test(NAME run.${name}
    COMMAND ${CMAKE_COMMAND}
      -DEXE=$<TARGET_FILE:t_${name}>
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

# Compile-failure tests: compiling tests/fail/<name>.prg must fail with the message in <name>.err.
file(GLOB XB_FAIL_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/fail ${CMAKE_CURRENT_SOURCE_DIR}/fail/*.prg)
foreach(t ${XB_FAIL_TESTS})
  string(REGEX REPLACE "\\.prg$" "" t ${t})
  add_test(NAME fail.${t}
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=$<TARGET_FILE:xbc>;${CMAKE_CURRENT_SOURCE_DIR}/fail/${t}.prg;-o;${CMAKE_CURRENT_BINARY_DIR}/fail_${t}.o"
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

# Code generation checks on cross targets: the IR xbc emits for <triple> must
# match a regular expression (tests/codegen_test.cmake).
function(xb_add_codegen_test name triple regex)
  add_test(NAME codegen.${name}
    COMMAND ${CMAKE_COMMAND}
      -DXBC=$<TARGET_FILE:xbc>
      -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/run/funcs.prg
      -DTRIPLE=${triple}
      -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
      "-DREGEX=${regex}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/codegen_test.cmake)
endfunction()

# 32-bit runtime arguments are extended as the target ABI requires.
xb_add_codegen_test(s390x_i32_zeroext s390x-linux-gnu "@xb_frame_push\\(ptr, i32 zeroext\\)")
xb_add_codegen_test(riscv64_i32_signext riscv64-linux-gnu "@xb_frame_push\\(ptr, i32 signext\\)")
xb_add_codegen_test(x86_64_i32_plain x86_64-linux-gnu "@xb_frame_push\\(ptr, i32\\)")

# A test that hangs or loops must fail quickly instead of exhausting the machine.
get_property(XB_ALL_TESTS DIRECTORY PROPERTY TESTS)
set_tests_properties(${XB_ALL_TESTS} PROPERTIES TIMEOUT 30)
