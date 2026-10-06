# libec as built by libec/LibEc.vcxproj. As on Windows it is compiled without
# LINK_SECUDE: SECUDE routines are called through the ECSecudeLib function
# table, which CrypToolApp fills from its SECUDE binding at start-up, so only
# the SECUDE headers are needed here. secude_compat (port/secude) is linked
# when that target exists, whichever directory defines it.

set(CT_EC_DIR "${CT_REPO_ROOT}/libec")
set(CT_EC_ALIAS_DIR "${CT_THIRDPARTY_ALIAS_ROOT}/ec")

set(CT_EC_SOURCES
  ECsecude.c EMSA1.C Ecssa.c s_bithdl.c s_ecDSA.c s_ecNR.c s_ecconv.c s_ecmath.c
  s_ecpX9c.c s_ecpara.c s_ecparp.c s_ecpcur.c s_ecppta.c s_ecpptc.c s_ecpptp.c
  s_ecprpt.c s_ecvali.c s_prng.c)
list(TRANSFORM CT_EC_SOURCES PREPEND "${CT_EC_DIR}/sources/")
set_source_files_properties(${CT_EC_SOURCES} PROPERTIES LANGUAGE C)

file(GLOB CT_EC_HEADERS "${CT_EC_DIR}/include/*")
ct_case_aliases(
  OUT_DIR "${CT_EC_ALIAS_DIR}"
  HEADER_DIRS "${CT_EC_DIR}/include"
  SCAN_FILES ${CT_EC_SOURCES} ${CT_EC_HEADERS} ${CT_APP_SCAN_FILES})

set(CT_SECUDE_INCLUDE_DIR "${CT_REPO_ROOT}/secude" CACHE PATH
  "Directory containing the SECUDE headers (secure.h, arithmet.h, secude/...)")

add_library(cryptool_ec STATIC ${CT_EC_SOURCES})
add_library(cryptool::ec ALIAS cryptool_ec)
ct_thirdparty_target(cryptool_ec)
target_include_directories(cryptool_ec SYSTEM PUBLIC
  "${CT_EC_DIR}/include"
  "${CT_EC_ALIAS_DIR}"
  "${CT_SECUDE_INCLUDE_DIR}")
set_target_properties(cryptool_ec PROPERTIES C_STANDARD 90 C_EXTENSIONS ON)
target_link_libraries(cryptool_ec PUBLIC $<TARGET_NAME_IF_EXISTS:secude_compat>)

ct_add_smoke_test(test_ec
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_ec.cpp"
  LIBS cryptool_ec
  DEFINES "CT_HAVE_SECUDE_COMPAT=$<TARGET_EXISTS:secude_compat>")
if(TARGET test_ec)
  set_tests_properties(test_ec PROPERTIES SKIP_RETURN_CODE 77)
endif()
