if(SH_EXEC AND TARGET mged AND TARGET asc2g)
  brlcad_add_test(NAME regress-comgeom COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/comgeom.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-comgeom "mged;asc2g;vdeck;comgeom-g" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged AND TARGET asc2g)

cmakefiles(
  comgeom.cg
  comgeom.sh
  comgeom.tgt-1-v4-documented.cg
  comgeom.tgt-1-v4.cg
  m35.cg.gz
)

# list of temporary files
set(
  comgeom_outfiles
  comgeom.log
  comgeom.m35-baseline.cg
  comgeom.m35.asc
  comgeom.m35.cg
  comgeom.m35.g
  comgeom.t-v4.g
  comgeom.t-v5.g
  region_ids
  regions
  solids
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${comgeom_outfiles}")
distclean(${comgeom_outfiles})

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
