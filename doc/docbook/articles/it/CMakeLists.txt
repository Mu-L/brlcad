set(articles_IT
  about_it.xml
  )

set(articles_IT_IMAGES
  images/bc2.gif
  images/brlcad-overview-benchmark-Bldg_391.jpg
  images/brlcad-overview-benchmark-M35.jpg
  images/brlcad-overview-benchmark-Moss.jpg
  images/brlcad-overview-benchmark-Sphflake.jpg
  images/brlcad-overview-benchmark-Star.jpg
  images/brlcad-overview-benchmark-World.jpg
  images/brlcad-overview-data-flow.png
  images/sflogo.png
  )

if (TARGET mged OR TARGET qged OR TARGET archer)
  if(BRLCAD_EXTRADOCS)
    ADD_DOC(articles_IT_IMAGES html/articles/it/images)
    ADD_DOCBOOK("HTML;PHP;PDF" articles_IT articles/it articles_IT_IMAGES_cp)
  endif(BRLCAD_EXTRADOCS)
endif (TARGET mged OR TARGET qged OR TARGET archer)

CMAKEFILES(
  CMakeLists.txt
  ${articles_IT}
  ${articles_IT_IMAGES}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
