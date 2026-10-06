# cryptool_gmp: system GMP (+ gmpxx) replacing the bundled MPIR binaries in libgmp/.
# Consumers must not put ${REPO_ROOT}/libgmp on their include path (MPIR headers).

if(TARGET cryptool_gmp)
  return()
endif()

set(_ct_gmp_hints "")
if(APPLE)
  if(NOT DEFINED CT_HOMEBREW_PREFIX)
    find_program(CT_BREW_EXECUTABLE brew)
    if(CT_BREW_EXECUTABLE)
      execute_process(COMMAND "${CT_BREW_EXECUTABLE}" --prefix
        OUTPUT_VARIABLE _ct_brew_prefix OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _ct_brew_rc)
      if(_ct_brew_rc EQUAL 0 AND _ct_brew_prefix)
        set(CT_HOMEBREW_PREFIX "${_ct_brew_prefix}" CACHE PATH "Homebrew prefix")
      endif()
    endif()
  endif()
  if(CT_HOMEBREW_PREFIX)
    list(APPEND _ct_gmp_hints "${CT_HOMEBREW_PREFIX}/opt/gmp" "${CT_HOMEBREW_PREFIX}")
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(CT_PC_GMP QUIET IMPORTED_TARGET GLOBAL gmp)
  pkg_check_modules(CT_PC_GMPXX QUIET IMPORTED_TARGET GLOBAL gmpxx)
endif()

add_library(cryptool_gmp INTERFACE)
add_library(cryptool::gmp ALIAS cryptool_gmp)

if(CT_PC_GMP_FOUND AND CT_PC_GMPXX_FOUND)
  target_link_libraries(cryptool_gmp INTERFACE PkgConfig::CT_PC_GMPXX PkgConfig::CT_PC_GMP)
  set(CT_GMP_VERSION "${CT_PC_GMP_VERSION}")
else()
  find_path(CT_GMP_INCLUDE_DIR NAMES gmp.h HINTS ${_ct_gmp_hints} PATH_SUFFIXES include)
  find_path(CT_GMPXX_INCLUDE_DIR NAMES gmpxx.h HINTS ${_ct_gmp_hints} PATH_SUFFIXES include)
  find_library(CT_GMP_LIBRARY NAMES gmp HINTS ${_ct_gmp_hints} PATH_SUFFIXES lib)
  find_library(CT_GMPXX_LIBRARY NAMES gmpxx HINTS ${_ct_gmp_hints} PATH_SUFFIXES lib)
  if(NOT CT_GMP_INCLUDE_DIR OR NOT CT_GMPXX_INCLUDE_DIR OR NOT CT_GMP_LIBRARY OR NOT CT_GMPXX_LIBRARY)
    message(FATAL_ERROR "GMP with C++ support (gmp.h, gmpxx.h, libgmp, libgmpxx) not found. "
      "Install libgmp-dev (Debian/Ubuntu) or 'brew install gmp' (macOS), or set CMAKE_PREFIX_PATH.")
  endif()
  target_include_directories(cryptool_gmp SYSTEM INTERFACE "${CT_GMP_INCLUDE_DIR}" "${CT_GMPXX_INCLUDE_DIR}")
  target_link_libraries(cryptool_gmp INTERFACE "${CT_GMPXX_LIBRARY}" "${CT_GMP_LIBRARY}")
  set(CT_GMP_VERSION "")
  file(STRINGS "${CT_GMP_INCLUDE_DIR}/gmp.h" _ct_gmp_ver REGEX "^#define __GNU_MP_VERSION(_MINOR|_PATCHLEVEL)? +[0-9]+")
  foreach(_l IN LISTS _ct_gmp_ver)
    if(_l MATCHES "([0-9]+)$")
      list(APPEND CT_GMP_VERSION "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  list(JOIN CT_GMP_VERSION "." CT_GMP_VERSION)
endif()

if(CT_GMP_VERSION AND CT_GMP_VERSION VERSION_LESS 6.0)
  message(WARNING "GMP ${CT_GMP_VERSION} found; GMP >= 6.0 is expected")
endif()
message(STATUS "cryptool_gmp: GMP ${CT_GMP_VERSION}")

ct_add_smoke_test(test_gmp
  SOURCES "${CT_THIRDPARTY_DIR}/tests/test_gmp.cpp"
  LIBS cryptool_gmp)
if(TARGET test_gmp)
  set_target_properties(test_gmp PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
endif()
