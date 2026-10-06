# cryptool_openssl_compat: replaces the bundled OpenSSL 1.0.1 headers/libeay32 used by
# CrypTool's HashingOperations.cpp ("OpenSSL//md2.h" ... "OpenSSL//sha.h", included
# inside namespace __SSL) and DlgAbout.cpp ("crypto.h" inside namespace OPENSSL).
# MD4/MD5/SHA-1/SHA-256/SHA-512/RIPEMD-160 forward to OpenSSL 3's libcrypto;
# MD2 and SHA-0 (absent from OpenSSL 3) are implemented here.
#
# The include directories are deliberately NOT marked SYSTEM: CrypTool's
# "OpenSSL//x.h" must resolve to these headers before OpenSSL 3's (imported,
# hence -isystem) include directory, which on case-insensitive file systems
# also matches "OpenSSL/". The compat headers keep OpenSSL 3's include guards
# and declare a superset of the real headers, so a real <openssl/sha.h> include
# that resolves here still works.

if(NOT TARGET OpenSSL::Crypto)
  if(APPLE AND NOT OPENSSL_ROOT_DIR AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
    find_program(CT_BREW_EXECUTABLE brew)
    if(CT_BREW_EXECUTABLE)
      execute_process(COMMAND "${CT_BREW_EXECUTABLE}" --prefix openssl@3
        OUTPUT_VARIABLE _ct_ossl_prefix OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _ct_ossl_rc)
      if(_ct_ossl_rc EQUAL 0 AND EXISTS "${_ct_ossl_prefix}/include/openssl/evp.h")
        set(OPENSSL_ROOT_DIR "${_ct_ossl_prefix}")
      endif()
    endif()
  endif()
  find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
endif()

set(CT_OPENSSL_COMPAT_DIR "${CT_THIRDPARTY_DIR}/openssl_compat")

add_library(cryptool_openssl_compat STATIC
  "${CT_OPENSSL_COMPAT_DIR}/src/ct_md2.c"
  "${CT_OPENSSL_COMPAT_DIR}/src/ct_sha0.c")
add_library(cryptool::openssl_compat ALIAS cryptool_openssl_compat)
target_include_directories(cryptool_openssl_compat PUBLIC
  "${CT_OPENSSL_COMPAT_DIR}/include"
  "${CT_OPENSSL_COMPAT_DIR}/include_bare")
target_link_libraries(cryptool_openssl_compat PUBLIC OpenSSL::Crypto)
set_target_properties(cryptool_openssl_compat PROPERTIES
  C_STANDARD 99 C_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ${CRYPTOOL_THIRDPARTY_PIC}
  FOLDER "thirdparty")

if(CRYPTOOL_THIRDPARTY_TESTS)
  # Real OpenSSL 3 headers only (no compat include dir), for layout comparison.
  add_library(test_openssl_compat_layout OBJECT "${CT_THIRDPARTY_DIR}/tests/test_openssl_compat_layout.c")
  target_link_libraries(test_openssl_compat_layout PRIVATE OpenSSL::Crypto)
  target_compile_definitions(test_openssl_compat_layout PRIVATE OPENSSL_SUPPRESS_DEPRECATED)
  set_target_properties(test_openssl_compat_layout PROPERTIES C_STANDARD 99 FOLDER "thirdparty/tests")
endif()
ct_add_smoke_test(test_openssl_compat
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_openssl_compat.cpp"
          $<TARGET_OBJECTS:test_openssl_compat_layout>
  LIBS cryptool_openssl_compat)
if(TARGET test_openssl_compat)
  set_target_properties(test_openssl_compat PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
endif()
