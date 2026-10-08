# NTL 5.5.2 as bundled in libNTL/ (pristine sources, the .c files renamed to .cpp).
# CrypTool relies on the NTL_vector_decl/NTL_matrix_decl macros that were removed
# in NTL 6, so the bundled version is built instead of a system NTL.

if(NOT CMAKE_C_COMPILER_LOADED)
  enable_language(C)
endif()

set(CT_NTL_DIR "${CT_REPO_ROOT}/libNTL")
set(CT_NTL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/ntl_generated")

# libNTL/include/NTL/mach_desc.h is NTL's mach_desc.win (32-bit long). The real
# description of the build host is produced by NTL's own MakeDesc program.
if(CMAKE_CROSSCOMPILING AND NOT CRYPTOOL_NTL_MACH_DESC)
  message(FATAL_ERROR "cryptool_ntl: cross compiling; run NTL's MakeDesc on the target and pass "
                      "-DCRYPTOOL_NTL_MACH_DESC=<path to mach_desc.h>")
endif()

set(CT_NTL_MACH_DESC "${CT_NTL_GENERATED_DIR}/NTL/mach_desc.h")
if(CRYPTOOL_NTL_MACH_DESC)
  configure_file("${CRYPTOOL_NTL_MACH_DESC}" "${CT_NTL_MACH_DESC}" COPYONLY)
elseif(NOT EXISTS "${CT_NTL_MACH_DESC}")
  set(_ntl_makedesc_dir "${CMAKE_CURRENT_BINARY_DIR}/ntl_makedesc")
  set(_ntl_makedesc_exe "${_ntl_makedesc_dir}/MakeDesc${CMAKE_EXECUTABLE_SUFFIX}")
  if(MSVC)
    set(_ntl_makedesc_flags "")
    set(_ntl_makedesc_libs "")
  else()
    set(_ntl_makedesc_flags -w -O0 -ffp-contract=off)
    set(_ntl_makedesc_libs m)
  endif()
  try_compile(_ntl_makedesc_ok "${_ntl_makedesc_dir}/build"
    SOURCES "${CT_THIRDPARTY_DIR}/ntl/MakeDesc.c" "${CT_THIRDPARTY_DIR}/ntl/MakeDescAux.c"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${CT_NTL_DIR}/include"
    COMPILE_DEFINITIONS ${_ntl_makedesc_flags}
    LINK_LIBRARIES ${_ntl_makedesc_libs}
    OUTPUT_VARIABLE _ntl_makedesc_log
    COPY_FILE "${_ntl_makedesc_exe}")
  if(NOT _ntl_makedesc_ok)
    message(FATAL_ERROR "cryptool_ntl: building NTL's MakeDesc failed:\n${_ntl_makedesc_log}")
  endif()
  file(MAKE_DIRECTORY "${CT_NTL_GENERATED_DIR}/NTL")
  execute_process(COMMAND "${_ntl_makedesc_exe}"
    WORKING_DIRECTORY "${CT_NTL_GENERATED_DIR}/NTL"
    RESULT_VARIABLE _ntl_makedesc_rc
    ERROR_VARIABLE _ntl_makedesc_err
    OUTPUT_QUIET)
  if(NOT _ntl_makedesc_rc EQUAL 0 OR NOT EXISTS "${CT_NTL_MACH_DESC}")
    file(REMOVE "${CT_NTL_MACH_DESC}")
    message(FATAL_ERROR "cryptool_ntl: NTL's MakeDesc failed:\n${_ntl_makedesc_err}")
  endif()
  string(REGEX MATCH "bits per long = [0-9]+" _ntl_bpl "${_ntl_makedesc_err}")
  message(STATUS "cryptool_ntl: generated NTL/mach_desc.h (${_ntl_bpl})")
endif()

set(CT_NTL_SOURCES
  FFT.cpp FacVec.cpp GF2.cpp GF2E.cpp GF2EX.cpp GF2EXFactoring.cpp GF2X.cpp GF2X1.cpp
  GF2XFactoring.cpp GF2XVec.cpp G_LLL_FP.cpp G_LLL_QP.cpp G_LLL_RR.cpp G_LLL_XD.cpp
  GetTime.cpp HNF.cpp LLL.cpp LLL_FP.cpp LLL_QP.cpp LLL_RR.cpp LLL_XD.cpp RR.cpp
  WordVector.cpp ZZ.cpp ZZVec.cpp ZZX.cpp ZZX1.cpp ZZXCharPoly.cpp ZZXFactoring.cpp
  ZZ_p.cpp ZZ_pE.cpp ZZ_pEX.cpp ZZ_pEXFactoring.cpp ZZ_pX.cpp ZZ_pX1.cpp
  ZZ_pXCharPoly.cpp ZZ_pXFactoring.cpp ctools.cpp fileio.cpp lip.cpp lzz_p.cpp
  lzz_pE.cpp lzz_pEX.cpp lzz_pEXFactoring.cpp lzz_pX.cpp lzz_pX1.cpp lzz_pXCharPoly.cpp
  lzz_pXFactoring.cpp mat_GF2.cpp mat_GF2E.cpp mat_RR.cpp mat_ZZ.cpp mat_ZZ_p.cpp
  mat_ZZ_pE.cpp mat_lzz_p.cpp mat_lzz_pE.cpp mat_poly_ZZ.cpp mat_poly_ZZ_p.cpp
  mat_poly_lzz_p.cpp pair_GF2EX_long.cpp pair_GF2X_long.cpp pair_ZZX_long.cpp
  pair_ZZ_pEX_long.cpp pair_ZZ_pX_long.cpp pair_lzz_pEX_long.cpp pair_lzz_pX_long.cpp
  quad_float.cpp tools.cpp vec_GF2.cpp vec_GF2E.cpp vec_GF2XVec.cpp vec_RR.cpp
  vec_ZZ.cpp vec_ZZVec.cpp vec_ZZ_p.cpp vec_ZZ_pE.cpp vec_double.cpp vec_long.cpp
  vec_lzz_p.cpp vec_lzz_pE.cpp vec_quad_float.cpp vec_ulong.cpp vec_vec_GF2.cpp
  vec_vec_GF2E.cpp vec_vec_RR.cpp vec_vec_ZZ.cpp vec_vec_ZZ_p.cpp vec_vec_ZZ_pE.cpp
  vec_vec_long.cpp vec_vec_lzz_p.cpp vec_vec_lzz_pE.cpp vec_vec_ulong.cpp
  vec_xdouble.cpp xdouble.cpp)
list(TRANSFORM CT_NTL_SOURCES PREPEND "${CT_NTL_DIR}/src/")

add_library(cryptool_ntl STATIC ${CT_NTL_SOURCES})
add_library(cryptool::ntl ALIAS cryptool_ntl)
ct_thirdparty_target(cryptool_ntl)
# The generated directory must precede libNTL/include so that <NTL/mach_desc.h>
# resolves to the generated header (also on case-insensitive file systems).
target_include_directories(cryptool_ntl BEFORE PUBLIC
  "$<BUILD_INTERFACE:${CT_NTL_GENERATED_DIR}>"
  "$<BUILD_INTERFACE:${CT_NTL_DIR}/include>")
target_compile_features(cryptool_ntl PUBLIC cxx_std_11)
set_target_properties(cryptool_ntl PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  # quad_float (LLL_QP/G_LLL_QP) needs exact IEEE double rounding; FMA contraction
  # (gcc's default on arm64) breaks its error-free transforms (NTL's QuadTest fails).
  target_compile_options(cryptool_ntl PRIVATE -ffp-contract=off)
  target_link_libraries(cryptool_ntl PUBLIC m)
endif()

ct_add_smoke_test(test_ntl
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_ntl.cpp"
  LIBS cryptool_ntl)
if(TARGET test_ntl)
  set_target_properties(test_ntl PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
endif()
