set(bn_headers
  anim.h
  complex.h
  defines.h
  dvec.h
  mat.h
  msr.h
  multipoly.h
  noise.h
  poly.h
  qmath.h
  rand.h
  randmt.h
  spm.h
  tabdata.h
  sobol.h
  str.h
  tol.h
  version.h
  wavelet.h
  )
BRLCAD_MANAGE_FILES(bn_headers ${INCLUDE_DIR}/brlcad/bn REQUIRED libbn)

CMAKEFILES(numgen.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
