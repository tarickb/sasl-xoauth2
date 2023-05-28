set(MD_TO_MAN
  sasl-xoauth2.conf.5)

find_program(PANDOC_PATH NAMES pandoc DOC "Path to pandoc, for Markdown-to-man-page conversion")
if(NOT EXISTS ${PANDOC_PATH})
  message(FATAL_ERROR "Unable to find pandoc")
else()
  message(STATUS "Found pandoc: ${PANDOC_PATH}")
endif()

foreach(man_page IN LISTS MD_TO_MAN)
  set(src ${CMAKE_CURRENT_SOURCE_DIR}/${man_page}.md)
  set(dst ${CMAKE_CURRENT_BINARY_DIR}/${man_page})

  if(NOT EXISTS "${src}")
    message(FATAL_ERROR "Unable to find Markdown at ${src} for ${man_page}")
  endif()

  if(man_page MATCHES "\\.([0-9])$")
    set(man_num ${CMAKE_MATCH_1})
  else()
    message(FATAL_ERROR "No man number in ${man_page}")
  endif()

  message(STATUS "[${src}] -> [${dst}] -> [${man_num}]")

  add_custom_command(
    OUTPUT ${dst}
    COMMAND ${PANDOC_PATH} -s -t man "${src}" -o "${dst}"
    DEPENDS ${src}
    COMMENT "Generating ${man_page}"
    VERBATIM)

  add_custom_target("${man_page}_man" ALL DEPENDS ${dst})
  install(
    FILES ${dst}
    DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man${man_num}"
    COMPONENT doc)
endforeach()
