include(${REPO_ROOT}/port/rc/rc2cpp.cmake)

set(CT_SRC ${REPO_ROOT}/CrypTool)
# *.CPP: the MIRACL wrapper copies (MONTY.CPP, ...); globbing is case-sensitive on Linux.
file(GLOB CT_SOURCES CONFIGURE_DEPENDS ${CT_SRC}/*.cpp ${CT_SRC}/*.CPP ${CT_SRC}/zip/*.c)
list(REMOVE_DUPLICATES CT_SOURCES)
list(FILTER CT_SOURCES EXCLUDE REGEX "/(Ber_Ell_Kurve|DlgCrypToolPRNGSeedCollector|DlgFindText|DlgHomophonicSubstitution|DlgMac|DlgSelHash|HomophonicSubstitution)\\.cpp$")
# zip/main.c is the standalone test driver of the zip code (not part of CrypTool.vcxproj).
list(FILTER CT_SOURCES EXCLUDE REGEX "/zip/main\\.c$")
# OLE-based picture control on Windows, wxWidgets-based elsewhere.
if(WIN32)
    list(FILTER CT_SOURCES EXCLUDE REGEX "/PictureEx(Loop)?_wx\\.cpp$")
else()
    list(FILTER CT_SOURCES EXCLUDE REGEX "/PictureEx(Loop)?\\.cpp$")
endif()
file(GLOB CT_EXTRA_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/libVolRen/ArcBall.cpp ${REPO_ROOT}/libVolRen/CMatrix.cpp ${REPO_ROOT}/libVolRen/CPosition.cpp
    ${REPO_ROOT}/libVolRen/CQuaternion.cpp ${REPO_ROOT}/libVolRen/CVector.cpp ${REPO_ROOT}/libVolRen/Volume.cpp
    ${REPO_ROOT}/libVolRen/VolumeUtilities.cpp ${REPO_ROOT}/ChallengeResponse/*.cpp)

add_executable(CrypTool ${CT_SOURCES} ${CT_EXTRA_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/platform.cpp)
mfcwx_add_rc(CrypTool RC ${CT_SRC}/CrypTool.rc MODULE CrypTool HEADERS ${CT_SRC}/resource.h)

set_target_properties(CrypTool PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON)
# Quoted includes only: <monty.h> etc. must resolve to the libraries, as on Windows.
target_compile_options(CrypTool PRIVATE "SHELL:-iquote ${CT_SRC}" "SHELL:-iquote ${REPO_ROOT}/libVolRen"
    "SHELL:-iquote ${REPO_ROOT}/ChallengeResponse")
target_include_directories(CrypTool PRIVATE ${REPO_ROOT}/scintilla/include)
target_link_libraries(CrypTool PRIVATE mfcwx cryptool_thirdparty $<TARGET_NAME_IF_EXISTS:secude_compat> OpenSSL::Crypto)
target_compile_options(CrypTool PRIVATE
    -Wno-inconsistent-missing-override -Wno-overloaded-virtual -Wno-deprecated-declarations
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-register -Wno-writable-strings -Wno-write-strings>
    -Wno-unused-variable -Wno-unused-but-set-variable -Wno-parentheses -Wno-dangling-else
    -Wno-switch -Wno-unused-value -Wno-sign-compare -Wno-char-subscripts -Wno-missing-braces
    -fno-delete-null-pointer-checks)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(CrypTool PRIVATE -Wno-c++11-narrowing -Wno-reserved-user-defined-literal
        -Wno-logical-op-parentheses -Wno-bitwise-op-parentheses -Wno-shift-op-parentheses
        -Wno-tautological-constant-out-of-range-compare -Wno-unused-private-field -Wno-null-conversion
        -Wno-undefined-bool-conversion -Wno-format-security)
else()
    target_compile_options(CrypTool PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-narrowing -fpermissive>)
endif()

set(CRYPTOOL_DEV_DATA_DIR ${CMAKE_BINARY_DIR}/data)
target_compile_definitions(CrypTool PRIVATE CRYPTOOL_DEV_DATA_DIR="${CRYPTOOL_DEV_DATA_DIR}")
add_custom_target(cryptool_data ALL
    COMMAND ${CMAKE_COMMAND} -DREPO_ROOT=${REPO_ROOT} -DDATA_DIR=${CRYPTOOL_DEV_DATA_DIR}
            -DMKPSE=$<TARGET_FILE:secude_compat_mkpse> -P ${CMAKE_CURRENT_SOURCE_DIR}/assemble_data.cmake
    DEPENDS secude_compat_mkpse
    COMMENT "Assembling the CrypTool data directory"
    VERBATIM)
add_dependencies(CrypTool cryptool_data)
