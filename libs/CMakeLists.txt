# The libraries xbase provides (docs/libraries.md): libs/<name>/<name>.xbprj, built by the build's xbc into
# <build>/libs/<name>/ (where xbc -L<build>/libs finds them) and installed with the runtime's module file. A library
# that uses others names them after its own name.
function(xb_add_library name)
  set(dir ${CMAKE_CURRENT_BINARY_DIR}/${name})
  # Named by CMake's convention for the toolchain, given to xbc (-o), so that both name it alike.
  set(archive ${dir}/${CMAKE_STATIC_LIBRARY_PREFIX}${name}${CMAKE_STATIC_LIBRARY_SUFFIX})
  set(uses "")
  foreach(other ${ARGN})
    list(APPEND uses xblib_${other})
  endforeach()
  xb_build_project(OUTPUTS ${archive} ${dir}/${name}.xbm DEPENDS ${uses}
                   ARGS ${CMAKE_CURRENT_SOURCE_DIR}/${name}/${name}.xbprj -o ${archive}
                   COMMENT "xbc ${name}.xbprj")
  add_custom_target(xblib_${name} ALL DEPENDS ${archive} ${dir}/${name}.xbm)
  install(FILES ${archive} ${dir}/${name}.xbm DESTINATION ${XB_INSTALL_MODULES})
endfunction()

xb_add_library(cli)
