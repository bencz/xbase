# The functions include/rtl.ch declares, for the scripts that read it
# (runtime/symbols.cmake, tests/declarations_test.cmake).
#
# xb_rtl_functions(<file> <names> <byref> [<reserved>]): in <names> the
# upper-cased name of every NATIVE declaration, in <byref> the matching mask of
# its parameters that receive references (bit k: parameter k; bit 31 also
# every parameter past it, so `@...` at position k sets bits k to 31), in
# <reserved> 1 for a RESERVED declaration, else 0.
function(xb_rtl_functions file out_names out_byref)
  file(READ ${file} text)
  string(REGEX REPLACE ";[ \t]*\r?\n[ \t]*" " " text "${text}") # a line ended by ';' goes on in the next one
  string(REGEX MATCHALL "\nEXTERN FUNCTION [^\n]*" lines "\n${text}") # declarations start a line
  set(names "")
  set(masks "")
  set(reserved "")
  foreach(line ${lines})
    if(NOT line MATCHES "\\)[^\n]*[ \t]NATIVE([ \t]|$)")
      continue()
    endif()
    string(REGEX REPLACE "\nEXTERN FUNCTION ([A-Za-z_][A-Za-z0-9_]*).*" "\\1" name "${line}")
    string(TOUPPER "${name}" name)
    string(REGEX REPLACE "^[^(]*\\(([^)]*)\\).*" "\\1" params "${line}")
    string(REPLACE "," ";" params "${params}")
    set(mask 0)
    set(k 0)
    foreach(p ${params})
      if(p MATCHES "@")
        if(p MATCHES "\\.\\.\\.")
          math(EXPR mask "${mask} | (4294967295 - ((1 << ${k}) - 1))")
        else()
          math(EXPR mask "${mask} | (1 << ${k})")
        endif()
      endif()
      math(EXPR k "${k} + 1")
    endforeach()
    list(APPEND names ${name})
    list(APPEND masks ${mask})
    if(line MATCHES "\\)[^\n]*[ \t]RESERVED([ \t]|$)")
      list(APPEND reserved 1)
    else()
      list(APPEND reserved 0)
    endif()
  endforeach()
  set(${out_names} ${names} PARENT_SCOPE)
  set(${out_byref} ${masks} PARENT_SCOPE)
  if(ARGC GREATER 3)
    set(${ARGV3} ${reserved} PARENT_SCOPE)
  endif()
endfunction()
