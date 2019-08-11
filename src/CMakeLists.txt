set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Werror")

find_package(PkgConfig REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(CURL REQUIRED)

pkg_check_modules(SASL REQUIRED "libsasl2")

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src)

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

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${CONFIG_FILE})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${CURL_INCLUDE_DIRS} jsoncpp_lib ${SASL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARIES} jsoncpp_lib)

install(
  TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION lib)

install(
  FILES ${CONFIG_FILE}
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
  COMPONENT config)

add_executable(${PROJECT_NAME}_test xoauth2_test.cc)
target_link_libraries(${PROJECT_NAME}_test ${PROJECT_NAME})

add_test(
  NAME ${PROJECT_NAME}_test
  COMMAND ${PROJECT_NAME}_test)
