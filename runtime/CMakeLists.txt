add_library(xbrt STATIC
  src/core/ctx.c
  src/core/display.c
  src/core/main.c
  src/core/numfmt.c
  src/core/ops.c
  src/core/set.c
  src/gc/gc.c
  src/types/array.c
  src/types/block.c
  src/types/string.c
  src/rtl/arrays.c
  src/rtl/console.c
  src/rtl/lang.c
)

if(WIN32)
  target_sources(xbrt PRIVATE src/platform/win32/platform.c)
else()
  target_sources(xbrt PRIVATE src/platform/posix/platform.c)
  find_package(Threads REQUIRED)
  target_link_libraries(xbrt PUBLIC m Threads::Threads)
endif()

target_include_directories(xbrt PUBLIC include PRIVATE src)
# set.ch is shared with xBase code, so setting numbers are defined once.
target_include_directories(xbrt PRIVATE ${PROJECT_SOURCE_DIR}/include)
xb_target_warnings(xbrt)
