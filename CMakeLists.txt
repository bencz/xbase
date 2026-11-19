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
# Each double operation rounded once, as xbc's code computes them (xbmath.h's double-double arithmetic depends on
# it): no contraction of a * b + c into a fused multiply-add, which GCC and Clang (clang-cl too) make by default on
# targets that have it (MSVC does not), and on 32-bit x86 (`x86_32`) SSE2, not the x87 unit, whose registers keep more
# bits between operations (MSVC uses SSE2 there by default).
function(xb_exact_doubles target x86_32)
  if(MSVC AND (CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    target_compile_options(${target} PRIVATE /clang:-ffp-contract=off)
  elseif(NOT MSVC)
    target_compile_options(${target} PRIVATE -ffp-contract=off)
    if(x86_32)
      target_compile_options(${target} PRIVATE -msse2 -mfpmath=sse)
    endif()
  endif()
endfunction()

function(xb_target_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${XB_WARNINGS_AS_ERRORS}>:/WX>)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                           $<$<BOOL:${XB_WARNINGS_AS_ERRORS}>:-Werror>)
  endif()
endfunction()

# The target of the xBase code the build compiles (the runtime's, the tests', the benchmarks'): the C compiler's
# target when it names one, else the host. A build whose C compiler targets something else through flags gives them
# as XB_TARGET_C_FLAGS, which the runtime and the programs linked with it get, and names the target (gcc -m32:
# -DXB_TARGET_C_FLAGS=-m32 -DXB_TARGET=i686-pc-linux-gnu); xbc, which compiles a C file of the runtime too, keeps the
# compilers' own flags, so C and C++ must build for one machine. A 32-bit C build on a 64-bit host names its target.
set(XB_TARGET "${CMAKE_C_COMPILER_TARGET}" CACHE STRING "target triple of the xBase code the build compiles")
set(XB_TARGET_C_FLAGS "" CACHE STRING "C compiler flags (a list) of the runtime's target, when not the host")
if(NOT CMAKE_C_SIZEOF_DATA_PTR EQUAL CMAKE_CXX_SIZEOF_DATA_PTR)
  message(FATAL_ERROR "the C and C++ compilers build for different machines (xbc compiles C too): give the runtime's "
                      "target flags as -DXB_TARGET_C_FLAGS=<flags> -DXB_TARGET=<triple> instead")
endif()
if(NOT XB_TARGET AND CMAKE_C_SIZEOF_DATA_PTR EQUAL 4 AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "64")
  message(FATAL_ERROR "the C compiler builds 32-bit code on a 64-bit host: name the target with -DXB_TARGET=<triple>")
endif()
if(XB_TARGET_C_FLAGS AND NOT XB_TARGET)
  message(FATAL_ERROR "XB_TARGET_C_FLAGS needs the target named with -DXB_TARGET=<triple>")
endif()

# Compiles an xBase source with xbc into an object file for XB_TARGET (extra
# xbc options in ARGN); used by the runtime, the tests and the benchmarks. The standard headers are named (-I): the
# build's xbc may hold no path to them (XB_SOURCE_INCLUDE off, a package's build).
function(xb_compile_prg src obj)
  get_filename_component(file ${src} NAME)
  add_custom_command(
    OUTPUT ${obj}
    COMMAND xbc ${src} -o ${obj} --depfile=${obj}.d -I${PROJECT_SOURCE_DIR}/include
            $<$<BOOL:${XB_TARGET}>:--target=${XB_TARGET}> ${ARGN}
    DEPENDS xbc ${src}
    DEPFILE ${obj}.d
    COMMENT "xbc ${file}")
  set_source_files_properties(${obj} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
endfunction()

# Where cmake --install puts things. On Windows a package's own folders (the release's zip: bin/, include/, libs/,
# docs/, README.md and LICENSE at the top); elsewhere the system's places (GNUInstallDirs: bin/, include/xbase/, lib/
# or lib64/, share/doc/xbase/ with README.md, LICENSE and docs/). README.md's links into docs/ hold in both. xbc finds
# the standard headers by the path from its folder to XB_INSTALL_INCLUDE (XB_INCLUDE_FROM_BIN: ../include in a package,
# ../include/xbase in a system's install, absolute install folders taken into account).
if(WIN32)
  set(XB_INSTALL_BIN bin)
  set(XB_INSTALL_INCLUDE include)
  set(XB_INSTALL_LIB libs)
  set(XB_INSTALL_DOC docs)
  set(XB_INSTALL_TOP .)
  file(RELATIVE_PATH XB_INCLUDE_FROM_BIN /prefix/${XB_INSTALL_BIN} /prefix/${XB_INSTALL_INCLUDE})
else()
  include(GNUInstallDirs)
  set(XB_INSTALL_BIN ${CMAKE_INSTALL_BINDIR})
  set(XB_INSTALL_INCLUDE ${CMAKE_INSTALL_INCLUDEDIR}/xbase)
  set(XB_INSTALL_LIB ${CMAKE_INSTALL_LIBDIR})
  set(XB_INSTALL_DOC ${CMAKE_INSTALL_DOCDIR}/docs)
  set(XB_INSTALL_TOP ${CMAKE_INSTALL_DOCDIR})
  file(RELATIVE_PATH XB_INCLUDE_FROM_BIN ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_INCLUDEDIR}/xbase)
endif()

add_subdirectory(runtime)
add_subdirectory(compiler)
add_subdirectory(bench)

# The standard headers (xbc finds them there by itself) beside the runtime's C headers, and the documentation.
install(DIRECTORY include/ DESTINATION ${XB_INSTALL_INCLUDE} FILES_MATCHING PATTERN "*.ch")
install(DIRECTORY docs/ DESTINATION ${XB_INSTALL_DOC})
install(FILES README.md LICENSE DESTINATION ${XB_INSTALL_TOP})

include(CTest)
if(BUILD_TESTING AND NOT XB_SOURCE_INCLUDE)
  message(FATAL_ERROR "the tests run the build's xbc, which finds the standard headers only with XB_SOURCE_INCLUDE: "
                      "turn it on, or BUILD_TESTING off")
endif()
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
