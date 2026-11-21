# Benchmarks: bench/<name>.prg is compiled with xbc (-O2), linked against xbrt
# and run by the `bench` target (not part of the default build or of ctest).
# Each program times its own work with ElapsedMs() and prints one line:
# `<name> <milliseconds> ok|WRONG`, after checking its result.
set(XB_BENCH_REPEAT 3 CACHE STRING "Runs of each benchmark; the best time is reported")

file(GLOB XB_BENCHES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.prg)
set(exes "")
set(targets "")
foreach(b ${XB_BENCHES})
  string(REGEX REPLACE "\\.prg$" "" name ${b})
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/${name}.o)
  xb_compile_prg(${CMAKE_CURRENT_SOURCE_DIR}/${b} ${obj} -O2)
  add_executable(bench_${name} EXCLUDE_FROM_ALL ${obj})
  set_target_properties(bench_${name} PROPERTIES LINKER_LANGUAGE C)
  target_link_libraries(bench_${name} PRIVATE xbrt)
  list(APPEND exes $<TARGET_FILE:bench_${name}>)
  list(APPEND targets bench_${name})
endforeach()

# The executables go through a file, one per configuration (multi-config generators): a list on the command line
# would be split.
set(list_file ${CMAKE_CURRENT_BINARY_DIR}/benches-$<CONFIG>.txt)
file(GENERATE OUTPUT ${list_file} CONTENT "$<JOIN:${exes},\n>\n")
add_custom_target(bench
  COMMAND ${CMAKE_COMMAND} -DLIST=${list_file} -DREPEAT=${XB_BENCH_REPEAT}
          -DBUILD_TYPE=$<CONFIG> -P ${CMAKE_CURRENT_SOURCE_DIR}/run_bench.cmake
  DEPENDS ${targets}
  USES_TERMINAL
  COMMENT "Running the benchmarks")

# The primitives: bench/primitives/<name>.prg times each primitive (nanoseconds per operation) and <name>.c the same
# work in C; the `primitives` target builds both and prints them side by side (docs/testing.md).
file(GLOB XB_PRIMITIVES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/primitives ${CMAKE_CURRENT_SOURCE_DIR}/primitives/*.prg)
set(pairs "")
set(targets "")
foreach(p ${XB_PRIMITIVES})
  string(REGEX REPLACE "\\.prg$" "" name ${p})
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/primitives_${name}.o)
  xb_compile_prg(${CMAKE_CURRENT_SOURCE_DIR}/primitives/${p} ${obj} -O2)
  add_executable(primitives_${name} EXCLUDE_FROM_ALL ${obj})
  set_target_properties(primitives_${name} PROPERTIES LINKER_LANGUAGE C)
  target_link_libraries(primitives_${name} PRIVATE xbrt)
  add_executable(primitives_${name}_c EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/primitives/${name}.c)
  target_compile_options(primitives_${name}_c PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-O2>)
  list(APPEND pairs "$<TARGET_FILE:primitives_${name}>|$<TARGET_FILE:primitives_${name}_c>")
  list(APPEND targets primitives_${name} primitives_${name}_c)
endforeach()
set(pairs_file ${CMAKE_CURRENT_BINARY_DIR}/primitives-$<CONFIG>.txt)
file(GENERATE OUTPUT ${pairs_file} CONTENT "$<JOIN:${pairs},\n>\n")
add_custom_target(primitives
  COMMAND ${CMAKE_COMMAND} -DLIST=${pairs_file} -P ${CMAKE_CURRENT_SOURCE_DIR}/primitives/run.cmake
  DEPENDS ${targets}
  USES_TERMINAL
  COMMENT "Running the primitives")
