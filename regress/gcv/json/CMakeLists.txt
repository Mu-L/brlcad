if(TARGET gcv-assetimport)
  gcv_regress_util("json" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "2744" EXPORTER)
endif(TARGET gcv-assetimport)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
