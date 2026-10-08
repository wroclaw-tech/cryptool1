# AES candidate ciphers compiled directly into CrypTool by CrypTool.vcxproj
# (..\AES\{Mars,RC6,Rijndael,Serpent,Twofish}). MARS, RC6 and Serpent assume a
# 32-bit long; the wrapper translation units in aes_candidates/ narrow long to
# int for those files only, and the generated public headers apply the same
# narrowing so the application sees identical struct layouts. The application
# should include "mars.h", "RC6.h", "Rijndael-api-fst.h", "Serpent.h" and
# "Twofish.h" from this target instead of the ..\AES\... paths.

# The directory is "aes" in git but "AES" in the Visual Studio project paths.
set(CT_AES_DIR "")
file(GLOB _ct_aes_root_entries RELATIVE "${CT_REPO_ROOT}" "${CT_REPO_ROOT}/*")
foreach(_ct_entry IN LISTS _ct_aes_root_entries)
  string(TOLOWER "${_ct_entry}" _ct_entry_lower)
  if(_ct_entry_lower STREQUAL "aes" AND IS_DIRECTORY "${CT_REPO_ROOT}/${_ct_entry}")
    set(CT_AES_DIR "${CT_REPO_ROOT}/${_ct_entry}")
  endif()
endforeach()
if(NOT CT_AES_DIR)
  message(FATAL_ERROR "AES candidate sources not found under ${CT_REPO_ROOT}")
endif()
set(CT_AES_SUPPORT_DIR "${CT_THIRDPARTY_DIR}/aes_candidates")
set(CT_AES_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/aes_candidates")
set(CT_AES_WRAPPER_DIR "${CT_AES_GEN_DIR}/include")
set(CT_AES_PUBLIC_ALIAS_DIR "${CT_AES_GEN_DIR}/aliases")
set(CT_AES_SOURCE_ALIAS_DIR "${CT_AES_GEN_DIR}/source_aliases")

configure_file("${CT_AES_SUPPORT_DIR}/ct_aes_prelude.h" "${CT_AES_WRAPPER_DIR}/ct_aes_prelude.h" COPYONLY)

function(_ct_aes_wrapper name original narrow)
  set(CT_WRAPPER_NAME "${name}")
  set(CT_WRAPPER_ORIGINAL "${CT_AES_DIR}/${original}")
  string(MAKE_C_IDENTIFIER "CT_AES_WRAPPER_${name}" CT_WRAPPER_GUARD)
  string(TOUPPER "${CT_WRAPPER_GUARD}" CT_WRAPPER_GUARD)
  if(narrow)
    set(CT_WRAPPER_NARROW_LONG "#define long int")
  else()
    set(CT_WRAPPER_NARROW_LONG "")
  endif()
  configure_file("${CT_AES_SUPPORT_DIR}/wrapper.h.in" "${CT_AES_WRAPPER_DIR}/${name}" @ONLY)
endfunction()

_ct_aes_wrapper(mars.h Mars/mars.h TRUE)
_ct_aes_wrapper(RC6.h RC6/rc6.h TRUE)
_ct_aes_wrapper(Rijndael-api-fst.h Rijndael/rijndael-api-fst.h FALSE)
_ct_aes_wrapper(Serpent.h Serpent/SERPENT.H TRUE)
_ct_aes_wrapper(Twofish.h Twofish/Twofish.h FALSE)

set(CT_AES_C_SOURCES
  "${CT_AES_DIR}/Mars/mars-opt.c" "${CT_AES_DIR}/RC6/rc6.c"
  "${CT_AES_DIR}/Rijndael/rijndael-alg-fst.c" "${CT_AES_DIR}/Rijndael/rijndael-api-fst.c"
  "${CT_AES_DIR}/Serpent/SERPENT.C" "${CT_AES_DIR}/Twofish/TWOFISH2.C")
file(GLOB CT_AES_C_HEADERS
  "${CT_AES_DIR}/Mars/*.h" "${CT_AES_DIR}/RC6/*.h" "${CT_AES_DIR}/Rijndael/*.h"
  "${CT_AES_DIR}/Serpent/*.H" "${CT_AES_DIR}/Serpent/*.h" "${CT_AES_DIR}/Twofish/*.H"
  "${CT_AES_DIR}/Twofish/*.h")

# Spellings the original sources use for their own headers (e.g. "Mars.h",
# "serpent.h", "platform.h"); Twofish.h needs "platform.h" in consumers too.
ct_case_aliases(
  OUT_DIR "${CT_AES_SOURCE_ALIAS_DIR}"
  HEADER_DIRS "${CT_AES_DIR}/Mars" "${CT_AES_DIR}/RC6" "${CT_AES_DIR}/Serpent" "${CT_AES_DIR}/Twofish"
  SCAN_FILES ${CT_AES_C_SOURCES} ${CT_AES_C_HEADERS}
  SCANNED_ONLY)
ct_case_aliases(
  OUT_DIR "${CT_AES_PUBLIC_ALIAS_DIR}"
  HEADER_DIRS "${CT_AES_WRAPPER_DIR}"
  EXCLUDE ct_aes_prelude.h)
ct_case_aliases(
  OUT_DIR "${CT_AES_PUBLIC_ALIAS_DIR}"
  HEADER_DIRS "${CT_AES_DIR}/Twofish"
  SCAN_FILES "${CT_AES_DIR}/Twofish/Twofish.h"
  SCANNED_ONLY)

add_library(cryptool_aes_candidates STATIC
  "${CT_AES_SUPPORT_DIR}/mars.c"
  "${CT_AES_SUPPORT_DIR}/rc6.c"
  "${CT_AES_DIR}/Rijndael/rijndael-alg-fst.c"
  "${CT_AES_DIR}/Rijndael/rijndael-api-fst.c"
  "${CT_AES_SUPPORT_DIR}/rijndael_sizes.c"
  "${CT_AES_SUPPORT_DIR}/serpent.c"
  "${CT_AES_SUPPORT_DIR}/twofish.c")
add_library(cryptool::aes_candidates ALIAS cryptool_aes_candidates)
ct_thirdparty_target(cryptool_aes_candidates)
set_target_properties(cryptool_aes_candidates PROPERTIES C_STANDARD 90 C_EXTENSIONS ON)
# rijndael-alg-fst.h defines "int ROUNDS;" in every including file (MSVC merges these).
if(NOT MSVC)
  target_compile_options(cryptool_aes_candidates PRIVATE -fcommon)
endif()
target_include_directories(cryptool_aes_candidates PRIVATE
  "${CT_AES_SUPPORT_DIR}"
  "${CT_AES_DIR}"
  "${CT_AES_SOURCE_ALIAS_DIR}")
target_include_directories(cryptool_aes_candidates SYSTEM PUBLIC
  "${CT_AES_WRAPPER_DIR}"
  "${CT_AES_PUBLIC_ALIAS_DIR}")

ct_add_smoke_test(test_aes_candidates
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_aes_candidates.cpp"
  LIBS cryptool_aes_candidates)
if(TARGET test_aes_candidates)
  set_target_properties(test_aes_candidates PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF)
endif()
