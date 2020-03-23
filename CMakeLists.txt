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

cmake_minimum_required(VERSION 3.0.0)
cmake_policy(VERSION 3.0)

execute_process(
  COMMAND head -n 1 ChangeLog
  COMMAND sed -e "s/.*(//" -e "s/).*//"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE CHANGELOG_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)

project(sasl-xoauth2 VERSION ${CHANGELOG_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)

option(EnableTests "Enable tests." ON)

if(EnableTests)
  enable_testing()
endif()
add_subdirectory(src)

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/packaging/;/packaging-build/;/packaging-output/;/packaging-scripts/;/.git/;.clang-format;~$;${CPACK_SOURCE_IGNORE_FILES}")

include(CPack)
