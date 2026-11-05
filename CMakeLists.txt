cmake_minimum_required(VERSION 3.20)
project(xbase VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(XB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# Warning policy for every xbase target (compiler, runtime, tools).
function(xb_target_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${XB_WARNINGS_AS_ERRORS}>:/WX>)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                           $<$<BOOL:${XB_WARNINGS_AS_ERRORS}>:-Werror>)
  endif()
endfunction()

# Compiles an xBase source with xbc into an object file (extra xbc options in
# ARGN); used by the tests and the benchmarks.
function(xb_compile_prg src obj)
  get_filename_component(file ${src} NAME)
  add_custom_command(
    OUTPUT ${obj}
    COMMAND xbc ${src} -o ${obj} --depfile=${obj}.d ${ARGN}
    DEPENDS xbc ${src}
    DEPFILE ${obj}.d
    COMMENT "xbc ${file}")
  set_source_files_properties(${obj} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
endfunction()

add_subdirectory(runtime)
add_subdirectory(compiler)
add_subdirectory(bench)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
