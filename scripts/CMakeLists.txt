# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(SCRIPTS
  sasl-xoauth2-tool)

find_program(ARGPARSE_MANPAGE_PATH NAMES argparse-manpage DOC "Path to argparse-manpage, to generate man pages from Python scripts")
if(NOT EXISTS ${ARGPARSE_MANPAGE_PATH})
  message(WARNING "Unable to find argparse-manpage, will not generate man pages for scripts")
else()
  message(STATUS "Found argparse-manpage: ${ARGPARSE_MANPAGE_PATH}")
endif()

foreach(script IN LISTS SCRIPTS)
  set(man_num 1)

  set(src ${CMAKE_CURRENT_SOURCE_DIR}/${script}.in)
  set(dst ${CMAKE_CURRENT_BINARY_DIR}/${script})
  set(man_base ${script}.${man_num})
  set(man ${CMAKE_CURRENT_BINARY_DIR}/${man_base})

  if(NOT EXISTS "${src}")
    message(FATAL_ERROR "Unable to find template at ${src} for ${script}")
  endif()

  configure_file(${src} ${dst})

  install(
    PROGRAMS ${dst}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT scripts)

  if(EXISTS ${ARGPARSE_MANPAGE_PATH})
    add_custom_command(
      OUTPUT ${man}
      COMMAND ${ARGPARSE_MANPAGE_PATH} --pyfile "${dst}" --function "argparse_get_parser" --author "Tarick Bedeir" --author-email "tarick@bedeir.com" --project-name "sasl-xoauth2" --url "https://github.com/tarickb/sasl-xoauth2" > "${man}"
      DEPENDS ${dst}
      COMMENT "Generating man page for ${script}"
      VERBATIM)

    add_custom_target("${man_base}_man" ALL DEPENDS ${man})
    install(
      FILES ${man}
      DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man${man_num}"
      COMPONENT doc)
  endif()
endforeach()
