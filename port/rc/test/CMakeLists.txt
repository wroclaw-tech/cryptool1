cmake_minimum_required(VERSION 3.16)
project(rc2cpp_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_LIST_DIR}/../rc2cpp.cmake")
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

add_executable(rc_test rc_test.cpp cryptool_checks.cpp numbershark_checks.cpp aestool_volren_checks.cpp)
target_include_directories(rc_test PRIVATE "${MFCWX_RC_INCLUDE_DIR}")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rc_test PRIVATE -Wall -Wextra -pedantic)
endif()

mfcwx_add_rc(rc_test RC "${ROOT}/CrypTool/CrypTool.rc" MODULE CrypTool HEADERS "${ROOT}/CrypTool/resource.h")
mfcwx_add_rc(rc_test RC "${ROOT}/aestool/aestool.rc" MODULE aestool HEADERS "${ROOT}/aestool/resource.h")
mfcwx_add_rc(rc_test RC "${ROOT}/NumberShark/MFC-ZahlenHai.rc" MODULE NumberShark HEADERS "${ROOT}/NumberShark/resource.h")
mfcwx_add_rc(rc_test RC "${ROOT}/libVolRen/VolRen.rc" MODULE VolRen HEADERS "${ROOT}/libVolRen/resource.h")

install(TARGETS rc_test RUNTIME DESTINATION bin)
mfcwx_install_rc_files(rc_test DESTINATION share/rc_test)

enable_testing()
add_test(NAME rc_test COMMAND rc_test)
