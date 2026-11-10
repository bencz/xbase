cmake_minimum_required(VERSION 3.20)
project(xbase VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Standard C, not the GNU dialect: floating point follows the standard
# (values with extra precision are rounded at assignments and casts).
set(CMAKE_C_EXTENSIONS OFF)
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

# The target of the xBase code the build compiles (the runtime's, the tests', the benchmarks'): the C compiler's
# target when it names one, else the host. A build whose C compiler targets something else through its flags names it
# (gcc -m32: -DXB_TARGET=i686-pc-linux-gnu); a 32-bit C build on a 64-bit host without it stops here.
set(XB_TARGET "${CMAKE_C_COMPILER_TARGET}" CACHE STRING "target triple of the xBase code the build compiles")
if(NOT XB_TARGET AND CMAKE_C_SIZEOF_DATA_PTR EQUAL 4 AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "64")
  message(FATAL_ERROR "the C compiler builds 32-bit code on a 64-bit host: name the target with -DXB_TARGET=<triple>")
endif()

# Compiles an xBase source with xbc into an object file for XB_TARGET (extra
# xbc options in ARGN); used by the runtime, the tests and the benchmarks.
function(xb_compile_prg src obj)
  get_filename_component(file ${src} NAME)
  add_custom_command(
    OUTPUT ${obj}
    COMMAND xbc ${src} -o ${obj} --depfile=${obj}.d $<$<BOOL:${XB_TARGET}>:--target=${XB_TARGET}> ${ARGN}
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
