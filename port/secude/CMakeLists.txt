cmake_minimum_required(VERSION 3.16)

project(secude_compat LANGUAGES C CXX)

get_filename_component(SECUDE_COMPAT_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

if(NOT TARGET OpenSSL::Crypto)
  find_package(OpenSSL 3 REQUIRED)
endif()

add_library(secude_compat STATIC
  src/alloc.cpp
  src/error.cpp
  src/der.cpp
  src/oids.cpp
  src/data_flags.cpp
  src/structs.cpp
  src/encode.cpp
  src/dname.cpp
  src/lnumber.cpp
  src/legacy_hash.cpp
  src/legacy_cipher.cpp
  src/provider.cpp
  src/hash.cpp
  src/symcipher.cpp
  src/asym.cpp
  src/crypt_api.cpp
  src/pse.cpp
  src/cert.cpp
  src/cadb.cpp
  src/pkcs12.cpp
  src/dump.cpp
  src/misc.cpp
  src/keystore.cpp
  src/symbols.cpp
)

target_compile_features(secude_compat PRIVATE cxx_std_17 PUBLIC cxx_std_11)
set_target_properties(secude_compat PROPERTIES
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)

target_include_directories(secude_compat
  PUBLIC
    "${SECUDE_COMPAT_REPO_ROOT}/secude"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(secude_compat PUBLIC OpenSSL::Crypto)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(secude_compat PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

add_executable(secude_compat_mkpse tools/mkpse.cpp)
target_link_libraries(secude_compat_mkpse PRIVATE secude_compat)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(_secude_compat_standalone ON)
else()
  set(_secude_compat_standalone OFF)
endif()
option(SECUDE_COMPAT_TESTS "Build the secude_compat tests" ${_secude_compat_standalone})

if(SECUDE_COMPAT_TESTS)
  enable_testing()
  # CrypTool's SecudeLib.h exactly as the app sees it (quote includes resolve next to the header first)
  add_library(secude_compat_test_cryptool_binding OBJECT tests/test_binding.cpp)
  target_include_directories(secude_compat_test_cryptool_binding PRIVATE
    "${SECUDE_COMPAT_REPO_ROOT}/CrypTool"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  target_link_libraries(secude_compat_test_cryptool_binding PRIVATE secude_compat)
  # the application is compiled as C++14
  set_target_properties(secude_compat_test_cryptool_binding PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(secude_compat_test_cryptool_binding PRIVATE -Wno-deprecated-register -Wno-register)
  endif()

  add_library(secude_compat_test_libec_binding OBJECT
    "${SECUDE_COMPAT_REPO_ROOT}/libec/sources/ECsecude.c"
    tests/test_libec_binding.c)
  target_include_directories(secude_compat_test_libec_binding PRIVATE
    "${SECUDE_COMPAT_REPO_ROOT}/libec/include")
  target_link_libraries(secude_compat_test_libec_binding PRIVATE secude_compat)
  set_target_properties(secude_compat_test_libec_binding PROPERTIES C_STANDARD 90 C_EXTENSIONS ON)

  add_executable(secude_compat_tests
    tests/main.cpp
    tests/test_hash.cpp
    tests/test_cipher.cpp
    tests/test_lnumber.cpp
    tests/test_encode.cpp
    tests/test_asym.cpp
    tests/test_pse.cpp
    tests/test_pkcs12.cpp
    $<TARGET_OBJECTS:secude_compat_test_cryptool_binding>
    $<TARGET_OBJECTS:secude_compat_test_libec_binding>)
  target_include_directories(secude_compat_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_features(secude_compat_tests PRIVATE cxx_std_17)
  target_link_libraries(secude_compat_tests PRIVATE secude_compat)

  add_test(NAME secude_compat_tests COMMAND secude_compat_tests)
  add_test(NAME secude_compat_tests_builtin_legacy COMMAND secude_compat_tests)
  set_tests_properties(secude_compat_tests_builtin_legacy PROPERTIES ENVIRONMENT "SECUDE_COMPAT_NO_LEGACY_PROVIDER=1")
endif()
