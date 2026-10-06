cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(CrypToolThirdParty LANGUAGES C CXX)
  set(CRYPTOOL_THIRDPARTY_STANDALONE ON)
else()
  set(CRYPTOOL_THIRDPARTY_STANDALONE OFF)
endif()

if(NOT DEFINED REPO_ROOT)
  get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()
set(CT_REPO_ROOT "${REPO_ROOT}")
set(CT_THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

option(CRYPTOOL_THIRDPARTY_TESTS "Build the third-party smoke tests" ${CRYPTOOL_THIRDPARTY_STANDALONE})
option(CRYPTOOL_THIRDPARTY_PIC "Build the third-party static libraries as position independent code" OFF)

if(CRYPTOOL_THIRDPARTY_TESTS)
  enable_testing()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(helpers)

# Pre-standard C++ headers (iostream.h, fstream.h, iomanip.h, strstrea.h, new.h).
add_library(cryptool_compat_headers INTERFACE)
target_include_directories(cryptool_compat_headers INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/compat_include")
add_library(cryptool::compat_headers ALIAS cryptool_compat_headers)

# Sources of the CrypTool application, scanned for #include spellings so the
# case-alias directories also cover the spellings used by the application.
file(GLOB CT_APP_SCAN_FILES
  "${CT_REPO_ROOT}/CrypTool/*.cpp" "${CT_REPO_ROOT}/CrypTool/*.CPP"
  "${CT_REPO_ROOT}/CrypTool/*.h" "${CT_REPO_ROOT}/CrypTool/*.H")

set(CT_ALL_PARTS gmp miracl analyse ntl ec cracklib apfloat cvact aes openssl)
set(CRYPTOOL_THIRDPARTY_PARTS "${CT_ALL_PARTS}" CACHE STRING "Third-party components to configure")

foreach(_part IN LISTS CT_ALL_PARTS)
  if(_part IN_LIST CRYPTOOL_THIRDPARTY_PARTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${_part}.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/${_part}.cmake")
  endif()
endforeach()

# Everything the CrypTool executable needs from this directory.
add_library(cryptool_thirdparty INTERFACE)
add_library(cryptool::thirdparty ALIAS cryptool_thirdparty)
foreach(_t IN ITEMS cryptool_analyse cryptool_miracl cryptool_ntl cryptool_ec cryptool_cracklib
               apfloat_compat cryptool_gmp cvact_compat cryptool_aes_candidates cryptool_openssl_compat)
  if(TARGET ${_t})
    target_link_libraries(cryptool_thirdparty INTERFACE ${_t})
  endif()
endforeach()
