# libanalyse as built by libanalyse/LibAnalyse.vcxproj. CrypTool's stdafx.h
# includes "libanalyse\la_string.h" and "libanalyse\analyse.h"; consumers get a
# generated directory providing libanalyse/<header> instead of the repository
# root (which would expose e.g. the repo's OpenSSL 1.0 headers).

set(CT_ANALYSE_DIR "${CT_REPO_ROOT}/libanalyse")
set(CT_ANALYSE_ALIAS_DIR "${CT_THIRDPARTY_ALIAS_ROOT}/analyse")
set(CT_ANALYSE_ROOT_DIR "${CT_THIRDPARTY_ALIAS_ROOT}/analyse_root")

set(CT_ANALYSE_SOURCES
  Chi2.cpp Cipher.cpp Converter.cpp Des.cpp FFT.CPP FreqTable.cpp LFSR.CPP
  MYMATH.CPP NGram.cpp OStream.cpp Permutation.cpp SBox.cpp StaticObjects.cpp
  String.cpp Symbol.cpp SymbolArray.cpp)
list(TRANSFORM CT_ANALYSE_SOURCES PREPEND "${CT_ANALYSE_DIR}/")
set_source_files_properties(${CT_ANALYSE_SOURCES} PROPERTIES LANGUAGE CXX)

file(GLOB CT_ANALYSE_HEADERS "${CT_ANALYSE_DIR}/*.h" "${CT_ANALYSE_DIR}/*.H")
# Only the case-mismatched spellings libanalyse itself uses (a blanket
# lowercase alias such as resource.h could shadow application headers).
ct_case_aliases(
  OUT_DIR "${CT_ANALYSE_ALIAS_DIR}"
  HEADER_DIRS "${CT_ANALYSE_DIR}"
  SCAN_FILES ${CT_ANALYSE_SOURCES} ${CT_ANALYSE_HEADERS}
  SCANNED_ONLY)
ct_case_aliases(
  OUT_DIR "${CT_ANALYSE_ROOT_DIR}/libanalyse"
  HEADER_DIRS "${CT_ANALYSE_DIR}"
  INCLUDE_EXACT)

add_library(cryptool_analyse STATIC ${CT_ANALYSE_SOURCES})
add_library(cryptool::analyse ALIAS cryptool_analyse)
ct_thirdparty_target(cryptool_analyse)
target_include_directories(cryptool_analyse SYSTEM PUBLIC
  "${CT_ANALYSE_ALIAS_DIR}"
  "${CT_ANALYSE_ROOT_DIR}")
target_link_libraries(cryptool_analyse PUBLIC cryptool_compat_headers)
set_target_properties(cryptool_analyse PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF)
target_compile_options(cryptool_analyse PRIVATE
  $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wno-register>)

ct_add_smoke_test(test_analyse
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_analyse.cpp"
  LIBS cryptool_analyse)
