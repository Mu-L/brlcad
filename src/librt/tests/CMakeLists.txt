brlcad_addexec(bundle bundle.c "librt" TEST)
brlcad_add_test(NAME rt_bundle COMMAND bundle)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")
distclean("${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")

brlcad_addexec(rt_pattern rt_pattern.c "librt" TEST)
brlcad_add_test(NAME rt_pattern_5 COMMAND rt_pattern 5)
set_property(
  DIRECTORY
  APPEND
  PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3"
)
distclean("${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")

brlcad_addexec(rt_dvec dvec.cpp "librt" NO_STRICT TEST)
brlcad_add_test(NAME rt_dvec COMMAND rt_dvec)

brlcad_addexec(rt_ssi ssi.cpp "librt" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
# BRLCAD_ADD_TEST(NAME rt_ssi COMMAND rt_ssi)

# NURBS testing
brlcad_addexec(rt_nurbs_tester nurbs_tests.cpp "librt;libbrep;libbu" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
#BRLCAD_ADD_TEST(NAME NURBS-get_closest_point-distinct_points nurbs_tester ${CMAKE_CURRENT_SOURCE_DIR}/nurbs_surfaces.g 1)

# materialX testing
#set(MATERIALX_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXCore.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXFormat.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXGenOsl.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXGenShader.lib)
set(USING_MATERIALX NO)
if(USING_MATERIALX)
  brlcad_addexec(materialx_test materialx_test.cpp "librt;" TEST)
  brlcad_add_test(NAME rt_mtlx_test COMMAND materialx_test)
else(USING_MATERIALX)
  cmakefiles(materialx_test.cpp)
endif(USING_MATERIALX)

# diff testing
brlcad_addexec(rt_diff diff.c "librt" TEST)

if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  brlcad_addexec(rt_binary_attribute binary_attribute.c "librt" TEST)
  brlcad_add_test(NAME rt_binary_attribute COMMAND rt_binary_attribute)
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)

# size testing
brlcad_addexec(db5_size db5_size.c "librt" TEST)

# datum testing
#BRLCAD_ADDEXEC(rt_datum rt_datum.c "librt" TEST)
#BRLCAD_ADD_TEST(NAME rt_datum_basic COMMAND rt_datum)

# perturb testing
#BRLCAD_ADDEXEC(rt_perturb rt_perturb.c "librt;libwdb" TEST)
#BRLCAD_ADD_TEST(NAME rt_perturb_basic COMMAND rt_perturb)

# cyclic path testing
brlcad_addexec(rt_cyclic cyclic.c "librt" TEST)
brlcad_add_test(NAME rt_cyclic_basic COMMAND rt_cyclic ${CMAKE_CURRENT_SOURCE_DIR}/cyclic_tests.g)

brlcad_addexec(rt_cache cache.cpp "librt" TEST)
brlcad_add_test(NAME rt_cache_serial_single_object COMMAND rt_cache 1)
brlcad_add_test(NAME rt_cache_parallel_single_object COMMAND rt_cache 2)
brlcad_add_test(NAME rt_cache_serial_multiple_identical_objects COMMAND rt_cache 3 10)
brlcad_add_test(NAME rt_cache_parallel_multiple_identical_objects  COMMAND rt_cache 4 10)
brlcad_add_test(NAME rt_cache_serial_multiple_different_objects COMMAND rt_cache 5 10)
brlcad_add_test(NAME rt_cache_parallel_multiple_different_objects  COMMAND rt_cache 6 10)
brlcad_add_test(NAME rt_cache_parallel_multiple_different_objects_hierarchy_1  COMMAND rt_cache 7 10)

# lod testing
brlcad_addexec(rt_lod lod.c "librt;libbg" TEST)

# bv_polygon <-> sketch testing
brlcad_addexec(rt_bv_poly_sketch bv_poly_sketch.c "librt;libbv" TEST)

# arb8 testing
brlcad_addexec(rt_arb8 arb8_tests.c "librt;libwdb;libbg;libbu" TEST)
#brlcad_add_test(NAME rt_arb8_tests COMMAND rt_arb8)

brlcad_addexec(bot_match bot_match.cpp "librt;libbg" TEST)

# boolweave testing
brlcad_addexec(rt_boolweave rt_boolweave.c "librt" TEST)

# Tests for primitive editing
add_subdirectory(edit)

set(
  distcheck_files
  CMakeLists.txt
  arb_intersect.g
  binary_attribute.c
  brep_boolean_tests.g
  cyclic_tests.g
  extreme_ssi_test.g
  matrix_tests.g
  nurbs_surfaces.g
  rt_datum.c
  rt_perturb.c
  sketch.g
)

cmakefiles(${distcheck_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
