BRLCAD_Find_Package(NETPBM REQUIRED)
BRLCAD_Find_Package(PNG REQUIRED)
BRLCAD_Find_Package(UTAHRLE REQUIRED)

set(LIBICV_SOURCES
  PImgHash.cpp
  fileformat.c
  rot.c
  color_space.c
  crop.c
  filter.c
  encoding.c
  operations.c
  pdiff.cpp
  stat.c
  size.c
  pix.c
  png.c
  ppm.c
  rle.c
  bw.c
  dpix.c
  )

# Note - libicv_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
set(ICV_LIBS
  ${libicv_deps}
  ${PNG_LIBRARIES}
  ${NETPBM_LIBRARY}
  ${UTAHRLE_LIBRARY}
  )
set(LOCAL_ICV_INCLUDE_DIRS
  ${PNG_INCLUDE_DIRS}
  ${NETPBM_INCLUDE_DIR}
  ${UTAHRLE_INCLUDE_DIR}
  )

BRLCAD_ADDLIB(libicv "${LIBICV_SOURCES}" "${ICV_LIBS}" "" "${LOCAL_ICV_INCLUDE_DIRS}")
set_target_properties(libicv PROPERTIES VERSION 20.0.1 SOVERSION 20)

if (HIDE_INTERNAL_SYMBOLS)
  if (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
    set_property(TARGET libicv APPEND PROPERTY COMPILE_DEFINITIONS "NETPBM_DLL_IMPORTS")
  endif (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
  if (TARGET libicv-obj)
    if (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
      set_property(TARGET libicv-obj APPEND PROPERTY COMPILE_DEFINITIONS "NETPBM_DLL_IMPORTS")
    endif (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
  endif (TARGET libicv-obj)
endif (HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

CMAKEFILES(
  CMakeLists.txt
  TODO
  PImgHash.h
  icv_private.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
