cmake_minimum_required(VERSION 3.10.0)
cmake_policy(VERSION 3.0)

project(sasl-xoauth2 VERSION 0.5 LANGUAGES CXX)

include(GNUInstallDirs)

option(EnableTests "Enable tests." ON)

if(EnableTests)
  enable_testing()
endif()
add_subdirectory(src)

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/debian/;/output/;/scripts/;/ubuntu-build/;/.git/;.clang-format;~$;${CPACK_SOURCE_IGNORE_FILES}")

include(CPack)
