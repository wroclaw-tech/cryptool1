# cvact_compat: replacement for the proprietary cv act library (libCVact), backed by OpenSSL 3.
if(NOT OpenSSL_FOUND AND APPLE AND NOT OPENSSL_ROOT_DIR AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
  foreach(_ct_ossl_hint /opt/homebrew/opt/openssl@3 /usr/local/opt/openssl@3)
    if(EXISTS "${_ct_ossl_hint}/include/openssl/evp.h")
      set(OPENSSL_ROOT_DIR "${_ct_ossl_hint}")
      break()
    endif()
  endforeach()
endif()
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

set(CT_CVACT_DIR "${CMAKE_CURRENT_LIST_DIR}/../cvact_compat")

add_library(cvact_compat STATIC
  "${CT_CVACT_DIR}/src/Algorithm.cpp"
  "${CT_CVACT_DIR}/src/Blob.cpp"
  "${CT_CVACT_DIR}/src/BlockCipherKey.cpp"
  "${CT_CVACT_DIR}/src/Date.cpp"
  "${CT_CVACT_DIR}/src/IESKey.cpp"
  "${CT_CVACT_DIR}/src/Key.cpp"
  "${CT_CVACT_DIR}/src/Tools.cpp")
add_library(cryptool::cvact_compat ALIAS cvact_compat)
target_include_directories(cvact_compat PUBLIC "${CT_CVACT_DIR}/include")
target_link_libraries(cvact_compat PUBLIC OpenSSL::Crypto)
target_compile_features(cvact_compat PUBLIC cxx_std_11 PRIVATE cxx_std_17)
target_compile_definitions(cvact_compat PRIVATE OPENSSL_API_COMPAT=30000 OPENSSL_NO_DEPRECATED)
set_target_properties(cvact_compat PROPERTIES
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ${CRYPTOOL_THIRDPARTY_PIC}
  FOLDER "thirdparty")

ct_add_smoke_test(test_cvact
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_cvact.cpp"
  LIBS cvact_compat)
