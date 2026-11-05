add_library(xbrt STATIC
  src/ctx.c
  src/item.c
  src/numfmt.c
  src/str.c
  src/ops.c
  src/console.c
  src/funcs.c
  src/main.c
)

if(WIN32)
  target_sources(xbrt PRIVATE src/platform_win32.c)
else()
  target_sources(xbrt PRIVATE src/platform_posix.c)
  target_link_libraries(xbrt PUBLIC m)
endif()

target_include_directories(xbrt PUBLIC include)
xb_target_warnings(xbrt)
