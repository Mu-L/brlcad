find_package_zlib(REQUIRED)

is_subpath("${CMAKE_BINARY_DIR}" "${ZLIB_LIBRARY}" LOCAL_TEST)
if(LOCAL_TEST OR TARGET ZLIB_BLD)
  add_definitions(-DZ_PREFIX)
endif(LOCAL_TEST OR TARGET ZLIB_BLD)

set(
  LIBRENDER_SRCS
  load.c
  load_g.c
  librender/camera.c
  librender/component.c
  librender/cut.c
  librender/depth.c
  librender/flat.c
  librender/flos.c
  librender/grid.c
  librender/hit.c
  librender/normal.c
  librender/path.c
  librender/phong.c
  librender/render_util.c
  librender/spall.c
  librender/surfel.c
  librender/texture_blend.c
  librender/texture_bump.c
  librender/texture_camo.c
  librender/texture_checker.c
  librender/texture_clouds.c
  librender/texture_gradient.c
  librender/texture_image.c
  librender/texture_mix.c
  librender/texture_perlin.c
  librender/texture_stack.c
)

set(
  ADRT_LOCAL_INCLUDE_DIRS
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/libcommon
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
)

brlcad_addlib(librender "${LIBRENDER_SRCS}" "libgcv;${ZLIB_LIBRARIES}" "" "${ADRT_LOCAL_INCLUDE_DIRS}")
set_target_properties(librender PROPERTIES VERSION 20.0.1 SOVERSION 20)
add_dependencies(librender gcv_plugins)

set(
  tie_HDRS
  adrt.h
  adrt_struct.h
  librender/camera.h
  librender/render.h
  librender/render_internal.h
  librender/render_util.h
  librender/texture.h
  librender/texture_internal.h
)
# TODO - should these be in brlcad/tie instead?
brlcad_manage_files(tie_HDRS ${INCLUDE_DIR}/tie)

# Tcl/Tk based ISST

if(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  set(isst_dirs ${OPENGL_INCLUDE_DIR_GL} ${OPENGL_INCLUDE_DIR_GLX})
  set(local_isst_dirs ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/librender)

  set(
    isst_libs
    libtclcad
    librender
    libdm
    ${TCL_LIBRARY}
    ${OPENGL_LIBRARIES}
    ${IMM32_LIBRARY}
    ${COMCTL32_LIBRARY}
    ${OPENGL32_LIBRARY}
  )

  brlcad_adddata(isst.tcl tclscripts/isst)
  brlcad_addexec(isst isst.c "${isst_libs}" GUI)
  if(isst_dirs)
    target_include_directories(isst PRIVATE ${isst_dirs})
  endif(isst_dirs)
  target_include_directories(isst BEFORE PRIVATE ${local_isst_dirs})
  add_target_deps(isst isst_tcl_cp TclIndexBld dm_plugins)
endif(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

cmakefiles(isst.tcl)
cmakefiles(isst.c)

add_subdirectory(master)
add_subdirectory(slave)

set(adrt_ignore_files load.h tienet.h)
cmakefiles(${adrt_ignore_files})
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
