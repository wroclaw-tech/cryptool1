# cryptool_cracklib: cracklib 2.7 (Win32 flavour used by CrypTool) as a static C library.
# No public include directory: CrypTool declares the API itself in passwordchecker.h.
# Runtime data: ${CT_CRACKLIB_DICTIONARY}.{pwd,pwi,hwm} must be shipped as words/cracklib_dict.*

set(_ct_cracklib_dir "${CT_REPO_ROOT}/cracklib-2.7/cracklib_Win32")
set(CT_CRACKLIB_DICTIONARY_DIR "${CT_REPO_ROOT}/setup/template/words")
set(CT_CRACKLIB_DICTIONARY "${CT_CRACKLIB_DICTIONARY_DIR}/cracklib_dict")

add_library(cryptool_cracklib STATIC
  "${_ct_cracklib_dir}/fascist.c"
  "${_ct_cracklib_dir}/packlib.c"
  "${_ct_cracklib_dir}/rules.c"
  "${_ct_cracklib_dir}/stringlib.c")
add_library(cryptool::cracklib ALIAS cryptool_cracklib)
target_compile_definitions(cryptool_cracklib PRIVATE IN_CRACKLIB)
# K&R function definitions: stay on gnu99 (they are invalid in C23).
set_target_properties(cryptool_cracklib PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
ct_thirdparty_target(cryptool_cracklib)

ct_add_smoke_test(test_cracklib
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_cracklib.cpp" "${CT_THIRDPARTY_DIR}/tests/test_cracklib_layout.c"
  LIBS cryptool_cracklib
  DEFINES "CT_CRACKLIB_DICT=\"${CT_CRACKLIB_DICTIONARY}\""
          "CT_CRACKLIB_PACKER_H=\"${_ct_cracklib_dir}/packer.h\"")
if(TARGET test_cracklib)
  set_target_properties(test_cracklib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
    C_STANDARD 99 C_EXTENSIONS ON)
endif()
