# EXE (a command line) succeeds and writes EXPECTED among its standard output.
execute_process(COMMAND ${EXE} OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "exit code ${rc}\n${err}")
endif()
string(FIND "${out}" "${EXPECTED}" at)
if(at EQUAL -1)
  message(FATAL_ERROR "not in the output: ${EXPECTED}\n--- output ---\n${out}")
endif()
