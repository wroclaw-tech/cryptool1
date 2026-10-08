# MIRACL (Shamus Software, ~2000) as built by libmiracl/LibMiracl.vcxproj:
# the C core plus the Big class. CrypTool compiles its own copies of the other
# C++ wrappers (MONTY, ELLIPTIC, FLASH, COMFLASH, POLY, FPOLY, POLYMOD) and
# libmiracl/source/CRT.CPP; CRT.CPP is included here as well because it is the
# only wrapper CrypTool takes from libmiracl/source.

set(CT_MIRACL_DIR "${CT_REPO_ROOT}/libmiracl")
set(CT_MIRACL_ALIAS_DIR "${CT_THIRDPARTY_ALIAS_ROOT}/miracl")

set(CT_MIRACL_C_SOURCES
  MRAES.C MRALLOC.C MRARTH0.C MRARTH1.C MRARTH2.C MRARTH3.C MRBRICK.C MRBUILD.C
  MRCOMBA.C MRCORE.C MRCRT.C MRCURVE.C MRDOUBLE.C MREBRICK.C MRECGF2M.C MRFAST.C
  MRFLASH.C MRFLSH1.C MRFLSH2.C MRFLSH3.C MRFLSH4.C MRFRND.C MRGCD.C MRIO1.C
  MRIO2.C MRJACK.C MRKCM.C MRLUCAS.C MRMONTY.C MRPI.C MRPOWER.C MRPRIME.C
  MRRAND.C MRROUND.C MRSCRT.C MRSHS.C MRSHS256.C MRSHS512.C MRSMALL.C MRSTRONG.C
  MRXGCD.C)
# MRMULDV.C (MSVC inline assembly) is replaced by the C versions in MRCORE.C (MR_NOASM).
set(CT_MIRACL_CXX_SOURCES BIG.CPP CRT.CPP)

list(TRANSFORM CT_MIRACL_C_SOURCES PREPEND "${CT_MIRACL_DIR}/source/")
list(TRANSFORM CT_MIRACL_CXX_SOURCES PREPEND "${CT_MIRACL_DIR}/source/")
set_source_files_properties(${CT_MIRACL_C_SOURCES} PROPERTIES LANGUAGE C)
set_source_files_properties(${CT_MIRACL_CXX_SOURCES} PROPERTIES LANGUAGE CXX)

ct_case_aliases(
  OUT_DIR "${CT_MIRACL_ALIAS_DIR}"
  HEADER_DIRS "${CT_THIRDPARTY_DIR}/miracl" "${CT_MIRACL_DIR}/include"
  SCAN_FILES ${CT_MIRACL_C_SOURCES} ${CT_MIRACL_CXX_SOURCES} ${CT_APP_SCAN_FILES})

add_library(cryptool_miracl STATIC ${CT_MIRACL_C_SOURCES} ${CT_MIRACL_CXX_SOURCES})
add_library(cryptool::miracl ALIAS cryptool_miracl)
ct_thirdparty_target(cryptool_miracl)
# The portable mirdef.h must shadow libmiracl/include/MIRDEF.H (Win32/__int64).
target_include_directories(cryptool_miracl SYSTEM PUBLIC
  "${CT_THIRDPARTY_DIR}/miracl"
  "${CT_MIRACL_ALIAS_DIR}"
  "${CT_MIRACL_DIR}/include")
target_link_libraries(cryptool_miracl PUBLIC cryptool_compat_headers)
set_target_properties(cryptool_miracl PROPERTIES
  C_STANDARD 90 C_EXTENSIONS ON
  CXX_STANDARD 17 CXX_EXTENSIONS OFF)
if(NOT WIN32)
  find_library(CT_LIBM m)
  if(CT_LIBM)
    target_link_libraries(cryptool_miracl PUBLIC ${CT_LIBM})
  endif()
endif()

ct_add_smoke_test(test_miracl
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_miracl.cpp"
  LIBS cryptool_miracl)
