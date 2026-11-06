# Compiles SOURCE for TRIPLE and checks the emitted LLVM IR against REGEX.
set(ir ${WORKDIR}/codegen_${NAME}.ll)
execute_process(
  COMMAND ${XBC} --target=${TRIPLE} ${SOURCE} -o ${WORKDIR}/codegen_${NAME}.o --dump-ir=${ir}
  ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xbc failed for ${TRIPLE}:\n${err}")
endif()
file(READ ${ir} text)
if(NOT text MATCHES "${REGEX}")
  message(FATAL_ERROR "IR for ${TRIPLE} does not match: ${REGEX}")
endif()
