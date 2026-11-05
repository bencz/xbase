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
