# Writes the runtime's symbol table (${OUT}): every NATIVE function
# include/rtl.ch (${RTL}) declares, by upper-cased name, with the mask of its
# parameters that receive references and whether it is RESERVED, for lookups
# by name at run time
# (core/symbols.c). The runtime.declarations test checks that rtl.ch and the
# xbf_ definitions match.
include(${CMAKE_CURRENT_LIST_DIR}/rtl_declarations.cmake)
xb_rtl_functions(${RTL} names masks reserved)
list(LENGTH names count)
set(text "/* Generated from include/rtl.ch by runtime/symbols.cmake. */\n#include \"xbrt_int.h\"\n\n")
foreach(n ${names})
  string(APPEND text "void xbf_${n}(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);\n")
endforeach()
string(APPEND text "\nconst xb_runtime_symbol xb_runtime_symbols[] = {\n")
set(k 0)
foreach(n ${names})
  list(GET masks ${k} m)
  list(GET reserved ${k} r)
  string(APPEND text "  {\"${n}\", xbf_${n}, ${m}u, ${r}},\n")
  math(EXPR k "${k} + 1")
endforeach()
string(APPEND text "};\nconst uint32_t xb_runtime_symbol_count = ${count};\n")
file(WRITE ${OUT}.tmp "${text}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OUT}.tmp ${OUT})
file(REMOVE ${OUT}.tmp)
