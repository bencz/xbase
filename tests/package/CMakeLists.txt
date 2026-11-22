# Builds a program with an installed xbase package, as its user would: XBASE_ROOT is the install's folder, a Windows
# package (bin/xbc, include/, libs/) or a system's install (bin/xbc, include/xbase/, lib/ or lib64/). xbc is given no
# include folder: it must find the standard headers in the install's include folder by itself, which the dependency
# file it writes shows. The release workflow (.github/workflows/release.yml) runs this on
# each package it makes: cmake -S tests/package -B <dir> -DXBASE_ROOT=<package>, build, ctest.
cmake_minimum_required(VERSION 3.20)
project(xbase_package_test C)

set(XBASE_ROOT "" CACHE PATH "the xbase package's folder")
# A package's xbc used for another target than its own system's (a cross build: the objects for XBC_TARGET, the
# program run through CMAKE_CROSSCOMPILING_EMULATOR).
set(XBC_TARGET "" CACHE STRING "the target triple xbc compiles for, when not its own system")
find_program(XBC xbc PATHS ${XBASE_ROOT}/bin NO_DEFAULT_PATH REQUIRED)
find_library(XBRT NAMES xbrt PATHS ${XBASE_ROOT}/libs ${XBASE_ROOT}/lib ${XBASE_ROOT}/lib64 NO_DEFAULT_PATH REQUIRED)
set(include ${XBASE_ROOT}/include)
if(EXISTS ${XBASE_ROOT}/include/xbase/std.ch)
  set(include ${XBASE_ROOT}/include/xbase)
endif()

set(sources ${CMAKE_CURRENT_SOURCE_DIR}/main.prg ${CMAKE_CURRENT_SOURCE_DIR}/greet.prg)
set(obj ${CMAKE_CURRENT_BINARY_DIR}/app${CMAKE_C_OUTPUT_EXTENSION})
set(deps ${CMAKE_CURRENT_BINARY_DIR}/app.d)
add_custom_command(OUTPUT ${obj}
                   COMMAND ${XBC} -c ${sources} -o ${obj} --depfile=${deps}
                           $<$<BOOL:${XBC_TARGET}>:--target=${XBC_TARGET}>
                   DEPENDS ${sources} COMMENT "xbc (no -I)")
add_executable(app ${obj})
set_target_properties(app PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(app PRIVATE ${XBRT})
if(WIN32)
  target_link_libraries(app PRIVATE shell32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(app PRIVATE m Threads::Threads)
endif()

# The same program from a project file, compiled and linked by xbc alone with the package's runtime and its cli library
# (their module files: docs/projects.md), but with the MSVC tools, whose link.exe only a developer prompt finds.
if(NOT MSVC)
  set(project ${CMAKE_CURRENT_BINARY_DIR}/project)
  add_custom_command(OUTPUT ${project}/app${CMAKE_EXECUTABLE_SUFFIX}
                     COMMAND ${XBC} ${CMAKE_CURRENT_SOURCE_DIR}/project.xbprj --out-dir=${project}
                             --depfile=${project}/app.d $<$<BOOL:${XBC_TARGET}>:--target=${XBC_TARGET}>
                     DEPENDS ${sources} ${CMAKE_CURRENT_SOURCE_DIR}/project.prg
                             ${CMAKE_CURRENT_SOURCE_DIR}/project.xbprj
                     COMMENT "xbc project.xbprj")
  add_custom_target(project ALL DEPENDS ${project}/app${CMAKE_EXECUTABLE_SUFFIX})
endif()

enable_testing()
if(NOT MSVC)
  add_test(NAME package.project
    COMMAND ${CMAKE_COMMAND} "-DEXE=${CMAKE_CROSSCOMPILING_EMULATOR};${project}/app${CMAKE_EXECUTABLE_SUFFIX}"
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/project_expected.txt
            -DDEPS=${project}/app.d -DINCLUDE=${include} -P ${CMAKE_CURRENT_SOURCE_DIR}/check.cmake)
endif()
add_test(NAME package
  COMMAND ${CMAKE_COMMAND} "-DEXE=${CMAKE_CROSSCOMPILING_EMULATOR};$<TARGET_FILE:app>"
          -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected.txt
          -DDEPS=${deps} -DINCLUDE=${include} -P ${CMAKE_CURRENT_SOURCE_DIR}/check.cmake)
