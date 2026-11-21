# Whatever order the system runs the modules' constructors in, a program's function is the one of its name and the
# libraries initialize first (xb_module_register's level): the program uses_greet linked with the objects of its
# libraries before its own (whose constructors then run first on ELF systems, last on MinGW) prints what
# projects/uses_greet.out holds. XBC, AR (the archiver, to take the objects out), LIBDIRS (where greet and cli are),
# SOURCE (uses_greet.prg), EXPECTED, TARGET (xbc's target or empty), WORKDIR.
set(dir ${WORKDIR}/library_order)
file(REMOVE_RECURSE ${dir})
file(MAKE_DIRECTORY ${dir})
set(target "")
if(TARGET)
  set(target --target=${TARGET})
endif()
set(dirs "")
set(objects "")
foreach(library greet cli)
  foreach(d ${LIBDIRS})
    if(EXISTS ${d}/${library}/lib${library}.a)
      execute_process(COMMAND ${AR} x ${d}/${library}/lib${library}.a WORKING_DIRECTORY ${dir} RESULT_VARIABLE rc)
      list(APPEND objects ${dir}/lib${library}.o)
      list(APPEND dirs -L${d})
    endif()
  endforeach()
endforeach()
execute_process(COMMAND ${XBC} ${SOURCE} --lib=greet ${dirs} -o ${dir}/program.o ${target} RESULT_VARIABLE rc
                ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "${err}")
endif()
execute_process(COMMAND ${XBC} ${objects} ${dir}/program.o --lib=greet ${dirs} --link -o ${dir}/program ${target}
                RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "${err}")
endif()
execute_process(COMMAND ${dir}/program OUTPUT_VARIABLE out RESULT_VARIABLE rc)
file(READ ${EXPECTED} expected)
if(NOT rc EQUAL 0 OR NOT out STREQUAL expected)
  message(FATAL_ERROR "exit code ${rc}\n--- expected ---\n${expected}\n--- actual ---\n${out}")
endif()
file(REMOVE_RECURSE ${dir})
