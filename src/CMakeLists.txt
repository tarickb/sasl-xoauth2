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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -Werror")

find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)

pkg_check_modules(JSON REQUIRED "jsoncpp")
pkg_check_modules(SASL REQUIRED "libsasl2")

include_directories(${CMAKE_SOURCE_DIR}/src)

set(SOURCES
  client.cc
  client.h
  config.cc
  config.h
  http.cc
  http.h
  log.cc
  log.h
  module.cc
  module.h
  token_store.cc
  token_store.h)

set(CONFIG_FILE ${PROJECT_NAME}.conf)
set(CONFIG_FILE_FULL_PATH ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${CONFIG_FILE})

link_directories(${JSON_LIBRARY_DIRS})
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${CONFIG_FILE})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${CURL_INCLUDE_DIRS} ${SASL_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARIES} ${JSON_LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PRIVATE CONFIG_FILE_FULL_PATH="${CONFIG_FILE_FULL_PATH}")

install(
  TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/sasl2)

install(
  FILES ${CONFIG_FILE}
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
  COMPONENT config)

add_executable(${PROJECT_NAME}_test xoauth2_test.cc)
target_link_libraries(${PROJECT_NAME}_test ${PROJECT_NAME})

add_test(
  NAME ${PROJECT_NAME}_test
  COMMAND ${PROJECT_NAME}_test)
