set(
  bu_headers
  app.h
  assert.h
  avs.h
  bitv.h
  cache.h
  cmd.h
  color.h
  cv.h
  debug.h
  defines.h
  dylib.h
  endian.h
  env.h
  exit.h
  file.h
  getopt.h
  glob.h
  hash.h
  hist.h
  hook.h
  interrupt.h
  list.h
  log.h
  magic.h
  malloc.h
  mapped_file.h
  mime.h
  observer.h
  opt.h
  parallel.h
  parse.h
  path.h
  process.h
  ptbl.h
  redblack.h
  simd.h
  snooze.h
  sort.h
  str.h
  tc.h
  time.h
  units.h
  user.h
  uuid.h
  version.h
  vfont.h
  vlb.h
  vls.h
)
brlcad_manage_files(bu_headers ${INCLUDE_DIR}/brlcad/bu REQUIRED libbu)

cmakefiles(CMakeLists.txt)
cmakefiles(column.h)
cmakefiles(tbl.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
