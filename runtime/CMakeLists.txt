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
  src/core/module.c
  src/core/names.c
  src/core/symbols.c
  src/core/numfmt.c
  src/core/ops.c
  src/core/set.c
  src/core/shared_file.c
  src/core/sync.c
  src/platform/blocking.c
  src/gc/gc.c
  src/types/array.c
  src/types/block.c
  src/types/class.c
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
  src/rtl/arrays.c
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
  src/rtl/transform.c
  src/macro/compile.c
  src/macro/lex.c
  src/macro/run.c
)

if(WIN32)
  target_sources(xbrt PRIVATE src/platform/win32/platform.c)
  target_link_libraries(xbrt PUBLIC shell32) # CommandLineToArgvW
else()
  target_sources(xbrt PRIVATE src/platform/posix/platform.c)
  find_package(Threads REQUIRED)
  target_link_libraries(xbrt PUBLIC m Threads::Threads)
  # Atomic operations of every size the runtime uses (platform/atomic.h); a target without native ones for some size
  # (older ARM, some 32-bit systems) gets them from libatomic.
  include(CheckCSourceCompiles)
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
  if(NOT XB_ATOMICS_NATIVE)
    target_link_libraries(xbrt PUBLIC atomic)
  endif()
endif()

# The runtime's functions written in xBase, compiled by the xbc of this build as modules of the runtime
# (xbc --runtime) for XB_TARGET (top-level CMakeLists.txt).
set(XB_RUNTIME_PRG src/rtl/collections.prg src/rtl/console.prg src/rtl/dates.prg src/rtl/errorsys.prg
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
# set.ch is shared with xBase code, so setting numbers are defined once.
target_include_directories(xbrt PRIVATE ${PROJECT_SOURCE_DIR}/include)
xb_target_warnings(xbrt)
# Volatile loads acquire and volatile stores release on every architecture (platform/atomic.h).
if(MSVC)
  target_compile_options(xbrt PRIVATE /volatile:ms)
endif()
