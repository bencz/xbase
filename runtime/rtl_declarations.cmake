# The functions include/rtl.ch declares, for the scripts that read it
# (runtime/symbols.cmake, tests/declarations_test.cmake).
#
# xb_rtl_functions(<file> <names> <byref>): in <names> the upper-cased name of
# every NATIVE declaration, in <byref> the matching mask of its parameters
# that receive references (bit k: parameter k; bit 31 also every parameter
# past it, so `@...` at position k sets bits k to 31).
function(xb_rtl_functions file out_names out_byref)
  file(READ ${file} text)
  string(REGEX MATCHALL "\nEXTERN FUNCTION [^\n]*" lines "\n${text}") # declarations start a line
  set(names "")
  set(masks "")
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
  endforeach()
  set(${out_names} ${names} PARENT_SCOPE)
  set(${out_byref} ${masks} PARENT_SCOPE)
endfunction()
