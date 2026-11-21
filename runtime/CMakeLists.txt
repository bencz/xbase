add_library(xbrt STATIC
  src/core/codepage.c
  src/core/codepage_data.c
  src/core/control.c
  src/core/ctx.c
  src/core/display.c
  src/core/error.c
  src/core/file.c
  src/core/language.c
  src/core/main.c
  src/core/memvar.c
  src/core/shared.c
  src/core/module.c
  src/core/names.c
  src/core/symbols.c
  src/core/numfmt.c
  src/core/trig.c
  src/core/ops.c
  src/core/set.c
  src/core/sched.c
  src/core/shared_file.c
  src/core/sync.c
  src/core/text.c
  src/platform/blocking.c
  src/platform/random.c
  src/gc/gc.c
  src/types/array.c
  src/types/block.c
  src/types/class.c
  src/types/members.c
  src/types/generator.c
  src/types/date.c
  src/types/hash.c
  src/types/object.c
  src/types/ref.c
  src/types/string.c
  src/rdd/area.c
  src/rdd/dbcmd.c
  src/rdd/dbf.c
  src/rdd/cdx.c
  src/rdd/dbt.c
  src/rdd/fpt.c
  src/rdd/ntx.c
  src/rdd/ordcmd.c
  src/rdd/relation.c
  src/rdd/text.c
  src/rdd/transfer.c
  src/rdd/order.c
  src/rdd/pages.c
  src/rdd/field.c
  src/rtl/agen.c
  src/rtl/arrays.c
  src/rtl/channels.c
  src/rtl/waiters.c
  src/rtl/collections.c
  src/rtl/console.c
  src/rtl/dates.c
  src/rtl/files.c
  src/rtl/hashes.c
  src/rtl/lang.c
  src/rtl/memfile.c
  src/rtl/memoline.c
  src/rtl/numbers.c
  src/rtl/objects.c
  src/rtl/strings.c
  src/rtl/tasks.c
  src/rtl/threads.c
  src/rtl/transform.c
  src/macro/compile.c
  src/macro/lex.c
  src/macro/run.c
)

# The system libraries the runtime needs, and so every program: names, or the linker's arguments (the runtime's module
# file, below, gives them to xbc's link step).
if(WIN32)
  target_sources(xbrt PRIVATE src/platform/win32/platform.c)
  set(XB_RUNTIME_SYSTEM_LIBS shell32) # CommandLineToArgvW
else()
  target_sources(xbrt PRIVATE src/platform/posix/platform.c)
  find_package(Threads REQUIRED)
  set(XB_RUNTIME_SYSTEM_LIBS m ${CMAKE_THREAD_LIBS_INIT})
  # Atomic operations of every size the runtime uses (platform/atomic.h); a target without native ones for some size
  # (older ARM, some 32-bit systems) gets them from libatomic.
  include(CheckCSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state()
  list(JOIN XB_TARGET_C_FLAGS " " CMAKE_REQUIRED_FLAGS) # a command line: the flags apart
  check_c_source_compiles("
    #include <stddef.h>
    #include <stdint.h>
    uint8_t b; uint32_t w; size_t s; void *p; _Alignas(8) uint64_t d;
    int main(void) {
      void *e = 0;
      return (int)(__atomic_fetch_or(&b, 1, __ATOMIC_ACQ_REL) + __atomic_fetch_add(&w, 1, __ATOMIC_ACQ_REL) +
                   __atomic_fetch_add(&s, 1, __ATOMIC_ACQ_REL) + (int)__atomic_fetch_add(&d, 1, __ATOMIC_ACQ_REL) +
                   __atomic_compare_exchange_n(&p, &e, &p, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    }" XB_ATOMICS_NATIVE)
  cmake_pop_check_state()
  if(NOT XB_ATOMICS_NATIVE)
    list(APPEND XB_RUNTIME_SYSTEM_LIBS atomic)
  endif()
endif()
target_link_libraries(xbrt PUBLIC ${XB_RUNTIME_SYSTEM_LIBS})

# The runtime's functions written in xBase, compiled by the xbc of this build as modules of the runtime
# (xbc --runtime) for XB_TARGET (top-level CMakeLists.txt).
set(XB_RUNTIME_PRG src/rtl/arrays.prg src/rtl/collections.prg src/rtl/console.prg src/rtl/dates.prg src/rtl/errorsys.prg
    src/rtl/files.prg src/rtl/languages.prg src/rtl/time.prg src/rdd/transfer.prg)
foreach(prg ${XB_RUNTIME_PRG})
  get_filename_component(stem ${prg} NAME_WE)
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/${stem}_prg.o)
  xb_compile_prg(${CMAKE_CURRENT_SOURCE_DIR}/${prg} ${obj} --runtime)
  target_sources(xbrt PRIVATE ${obj})
endforeach()

# The runtime's functions by name (runtime/symbols.cmake), from include/rtl.ch.
set(XB_RUNTIME_SYMBOLS ${CMAKE_CURRENT_BINARY_DIR}/rtl_symbols.c)
add_custom_command(OUTPUT ${XB_RUNTIME_SYMBOLS}
  COMMAND ${CMAKE_COMMAND} -DRTL=${PROJECT_SOURCE_DIR}/include/rtl.ch -DOUT=${XB_RUNTIME_SYMBOLS}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/symbols.cmake
  DEPENDS ${PROJECT_SOURCE_DIR}/include/rtl.ch ${CMAKE_CURRENT_SOURCE_DIR}/symbols.cmake
          ${CMAKE_CURRENT_SOURCE_DIR}/rtl_declarations.cmake
  COMMENT "runtime symbol table")
target_sources(xbrt PRIVATE ${XB_RUNTIME_SYMBOLS})

target_include_directories(xbrt PUBLIC include PRIVATE src)
# The target's flags (top-level CMakeLists.txt), for the runtime and every program linked with it.
target_compile_options(xbrt PUBLIC ${XB_TARGET_C_FLAGS})
target_link_options(xbrt PUBLIC ${XB_TARGET_C_FLAGS})
# set.ch is shared with xBase code, so setting numbers are defined once.
target_include_directories(xbrt PRIVATE ${PROJECT_SOURCE_DIR}/include)
xb_target_warnings(xbrt)
# The runtime's target is 32-bit x86: named, or the host's when none is.
set(XB_TARGET_X86_32 OFF)
if(XB_TARGET MATCHES "^i[3-6]86" OR
   (NOT XB_TARGET AND CMAKE_C_SIZEOF_DATA_PTR EQUAL 4 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$"))
  set(XB_TARGET_X86_32 ON)
endif()
xb_exact_doubles(xbrt ${XB_TARGET_X86_32})
# Volatile loads acquire and volatile stores release on every architecture (platform/atomic.h).
if(MSVC)
  target_compile_options(xbrt PRIVATE /volatile:ms)
endif()

# The runtime's module file (docs/projects.md), which xbc's link step reads: the archive, the system libraries, the
# linker and its flags. The build tree's beside the archive, with the C compiler that built the runtime and the flags
# it links programs with (the target's, the toolchain's own: a sanitizer, a sysroot); the installed one in the modules'
# folder, with the compiler's name (found where the program is built) and the target's flags only.
foreach(define XB_ABI_VERSION XB_MODULE_FILE_FORMAT)
  file(STRINGS include/xbrt.h line REGEX "^#define ${define} [0-9]+u?$")
  string(REGEX MATCH "[0-9]+" ${define} "${line}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS include/xbrt.h)
string(TOUPPER ${XB_RUNTIME_MODULE} XB_RUNTIME_LIBRARY)
set(XB_INSTALLED_FLAGS ${XB_TARGET_C_FLAGS})
if(MSVC)
  set(XB_LINK_STYLE msvc)
  set(XB_LINKER link)
  set(XB_INSTALLED_LINKER link)
  set(XB_BUILD_FLAGS)
else()
  set(XB_LINK_STYLE gnu)
  set(XB_LINKER ${CMAKE_C_COMPILER})
  get_filename_component(XB_INSTALLED_LINKER ${CMAKE_C_COMPILER} NAME)
  if(CMAKE_C_COMPILER_TARGET)
    list(APPEND XB_INSTALLED_FLAGS --target=${CMAKE_C_COMPILER_TARGET})
  endif()
  separate_arguments(XB_BUILD_FLAGS UNIX_COMMAND "${CMAKE_C_FLAGS}")
  if(CMAKE_SYSROOT)
    list(APPEND XB_BUILD_FLAGS --sysroot=${CMAKE_SYSROOT})
  endif()
endif()
separate_arguments(XB_LINKER_FLAGS NATIVE_COMMAND "${CMAKE_EXE_LINKER_FLAGS}")
set(XB_BUILD_FLAGS ${XB_INSTALLED_FLAGS} ${XB_BUILD_FLAGS} ${XB_LINKER_FLAGS})
# A list as the module file's words: each quoted, so that one may hold blanks.
function(xb_module_words out)
  set(words "")
  foreach(word ${ARGN})
    string(APPEND words " \"${word}\"")
  endforeach()
  set(${out} "${words}" PARENT_SCOPE)
endfunction()
xb_module_words(XB_SYSTEM_WORDS ${XB_RUNTIME_SYSTEM_LIBS})
function(xb_runtime_module_file out archive linker)
  xb_module_words(flags ${ARGN})
  file(GENERATE OUTPUT ${out} CONTENT
"# The module file of the xbase runtime, written by its build (docs/projects.md): how programs link with it.
format = ${XB_MODULE_FILE_FORMAT}
library = ${XB_RUNTIME_LIBRARY}
abi = ${XB_ABI_VERSION}
target = ${XB_TARGET}
archive = ${archive}
linker = ${linker}
style = ${XB_LINK_STYLE}
flags =${flags}
system =${XB_SYSTEM_WORDS}
")
endfunction()
xb_runtime_module_file($<TARGET_FILE_DIR:xbrt>/${XB_RUNTIME_MODULE}.xbm $<TARGET_FILE_NAME:xbrt> ${XB_LINKER}
                       ${XB_BUILD_FLAGS})
file(RELATIVE_PATH XB_ARCHIVE_FROM_MODULES /prefix/${XB_INSTALL_MODULES} /prefix/${XB_INSTALL_LIB})
if(XB_ARCHIVE_FROM_MODULES)
  string(REGEX REPLACE "/+$" "" XB_ARCHIVE_FROM_MODULES ${XB_ARCHIVE_FROM_MODULES})
  string(APPEND XB_ARCHIVE_FROM_MODULES /)
endif()
xb_runtime_module_file(${CMAKE_CURRENT_BINARY_DIR}/install/${XB_RUNTIME_MODULE}.xbm
                       ${XB_ARCHIVE_FROM_MODULES}$<TARGET_FILE_NAME:xbrt> ${XB_INSTALLED_LINKER} ${XB_INSTALLED_FLAGS})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/install/${XB_RUNTIME_MODULE}.xbm DESTINATION ${XB_INSTALL_MODULES})

# The package's runtime library and the C headers a NATIVE function written in C includes.
install(TARGETS xbrt ARCHIVE DESTINATION ${XB_INSTALL_LIB})
install(FILES include/xbrt.h include/xbmath.h include/xbclass.h DESTINATION ${XB_INSTALL_INCLUDE})
