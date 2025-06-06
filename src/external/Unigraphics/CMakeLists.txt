if(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)
  # Minimum required version of CMake
  cmake_minimum_required(VERSION 3.20)

  # Set CMake project name
  project(UG2G)
else(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)
  set(ug_g_srcs ug-g.c ug_misc.c shim.cpp)

  if(BRLCAD_ENABLE_TCL)
    set(
      UG_DIRS
      "${CMAKE_CURRENT_BINARY_DIR}/../../../include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
      "${OPENNURBS_INCLUDE_DIRS}"
      ${ZLIB_INCLUDE_DIRS}
      ${REGEX_INCLUDE_DIRS}
      ${TCL_INCLUDE_PATH}
    )

    # The build shims aren't so great at mocking the initialization
    # behavior of the real code.
    check_cxx_flag("Wno-uninitialized")
    check_cxx_flag("Wno-maybe-uninitialized")

    add_definitions(-DTEST_BUILD)
    add_library(ug-g ${ug_g_srcs})
    target_include_directories(ug-g BEFORE PRIVATE ${UG_DIRS})
    target_link_libraries(
      ug-g
      librt
      libwdb
      libbrep
      libbg
      libnmg
      libbv
      libbn
      libbu
      ${TCL_LIBRARY}
    )
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
    set_target_properties(ug-g PROPERTIES FOLDER "BRL-CAD Executables/Build Only")
    if(HIDE_INTERNAL_SYMBOLS)
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BU_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BN_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BV_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "NMG_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BG_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BREP_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "RT_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "WDB_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "OPENNURBS_IMPORTS")
      set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "TINYCTHREAD_DLL_IMPORTS")
    endif(HIDE_INTERNAL_SYMBOLS)
  endif(BRLCAD_ENABLE_TCL)

  # Distcheck file list
  cmakefiles(${ug_g_srcs} conv.h log.h shim.h ug-g.1 ug_misc.h)
  cmakefiles(CMakeLists.txt)
endif(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
