include(${REPO_ROOT}/port/rc/rc2cpp.cmake)

set(CT_SRC ${REPO_ROOT}/CrypTool)
file(GLOB CT_SOURCES CONFIGURE_DEPENDS ${CT_SRC}/*.cpp ${CT_SRC}/zip/*.c)
list(FILTER CT_SOURCES EXCLUDE REGEX "/(Ber_Ell_Kurve|DlgCrypToolPRNGSeedCollector|DlgFindText|DlgHomophonicSubstitution|DlgMac|DlgSelHash|HomophonicSubstitution)\\.cpp$")
file(GLOB CT_EXTRA_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/libVolRen/ArcBall.cpp ${REPO_ROOT}/libVolRen/CMatrix.cpp ${REPO_ROOT}/libVolRen/CPosition.cpp
    ${REPO_ROOT}/libVolRen/CQuaternion.cpp ${REPO_ROOT}/libVolRen/CVector.cpp ${REPO_ROOT}/libVolRen/Volume.cpp
    ${REPO_ROOT}/libVolRen/VolumeUtilities.cpp ${REPO_ROOT}/ChallengeResponse/*.cpp)

add_executable(CrypTool ${CT_SOURCES} ${CT_EXTRA_SOURCES})
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
