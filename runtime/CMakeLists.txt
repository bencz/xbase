add_library(xbrt STATIC
  src/core/control.c
  src/core/ctx.c
  src/core/display.c
  src/core/error.c
  src/core/main.c
  src/core/module.c
  src/core/symbols.c
  src/core/numfmt.c
  src/core/ops.c
  src/core/set.c
  src/gc/gc.c
  src/types/array.c
  src/types/block.c
  src/types/date.c
  src/types/hash.c
  src/types/object.c
  src/types/ref.c
  src/types/string.c
  src/rtl/arrays.c
  src/rtl/collections.c
  src/rtl/console.c
  src/rtl/dates.c
  src/rtl/hashes.c
  src/rtl/lang.c
  src/rtl/numbers.c
  src/rtl/strings.c
)

if(WIN32)
  target_sources(xbrt PRIVATE src/platform/win32/platform.c)
  target_link_libraries(xbrt PUBLIC shell32) # CommandLineToArgvW
else()
  target_sources(xbrt PRIVATE src/platform/posix/platform.c)
  find_package(Threads REQUIRED)
  target_link_libraries(xbrt PUBLIC m Threads::Threads)
endif()

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
