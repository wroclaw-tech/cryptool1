# apfloat_compat: GMP-backed replacement for the prebuilt apfloat 2.41 Windows
# libraries in apfloat/. Consumers use this include directory instead of apfloat/.

if(NOT TARGET cryptool_gmp)
  include("${CMAKE_CURRENT_LIST_DIR}/gmp.cmake")
endif()

add_library(apfloat_compat STATIC
  "${CT_THIRDPARTY_DIR}/apfloat_compat/src/apint.cpp"
  "${CT_THIRDPARTY_DIR}/apfloat_compat/include/ap.h"
  "${CT_THIRDPARTY_DIR}/apfloat_compat/include/apint.h")
add_library(cryptool::apfloat ALIAS apfloat_compat)
target_include_directories(apfloat_compat PUBLIC "${CT_THIRDPARTY_DIR}/apfloat_compat/include")
target_link_libraries(apfloat_compat PUBLIC cryptool_gmp)
target_compile_features(apfloat_compat PUBLIC cxx_std_11)
set_target_properties(apfloat_compat PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ${CRYPTOOL_THIRDPARTY_PIC}
  FOLDER "thirdparty")

ct_add_smoke_test(test_apfloat
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_apfloat.cpp"
  LIBS apfloat_compat)
if(TARGET test_apfloat)
  set_target_properties(test_apfloat PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  find_package(Threads)
  if(Threads_FOUND)
    target_link_libraries(test_apfloat PRIVATE Threads::Threads)
  endif()
endif()
