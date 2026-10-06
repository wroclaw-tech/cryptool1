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

# ---------------------------------------------------------------------------------------------
# Installation and packages

include(GNUInstallDirs)
if(APPLE)
    set(MACOSX_BUNDLE_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icons/CrypTool.icns)
    set_source_files_properties(${MACOSX_BUNDLE_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(CrypTool PRIVATE ${MACOSX_BUNDLE_ICON})
    set_target_properties(CrypTool PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in)
    set(CRYPTOOL_DATA_DEST CrypTool.app/Contents/Resources)
    install(TARGETS CrypTool BUNDLE DESTINATION .)
else()
    set(CRYPTOOL_DATA_DEST ${CMAKE_INSTALL_DATADIR}/CrypTool)
    install(TARGETS CrypTool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES cryptool.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/cryptool-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME cryptool.png)
    install(FILES icons/cryptool-48.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps RENAME cryptool.png)
endif()

mfcwx_install_rc_files(CrypTool DESTINATION ${CRYPTOOL_DATA_DEST})
foreach(help hlp_en hlp_de)
    install(DIRECTORY ${CT_SRC}/${help} DESTINATION ${CRYPTOOL_DATA_DEST}
            PATTERN "*.vcproj" EXCLUDE PATTERN "*.vcxproj*" EXCLUDE PATTERN "*.hhp" EXCLUDE)
endforeach()
file(GLOB CT_TEMPLATE_ENTRIES RELATIVE ${REPO_ROOT}/setup/template ${REPO_ROOT}/setup/template/*)
foreach(entry IN LISTS CT_TEMPLATE_ENTRIES)
    string(TOLOWER "${entry}" lower)
    if(lower MATCHES "\\.(dll|manifest)$" OR lower STREQUAL "pse")
        continue()
    endif()
    if(IS_DIRECTORY ${REPO_ROOT}/setup/template/${entry})
        install(DIRECTORY ${REPO_ROOT}/setup/template/${entry} DESTINATION ${CRYPTOOL_DATA_DEST})
    else()
        install(FILES ${REPO_ROOT}/setup/template/${entry} DESTINATION ${CRYPTOOL_DATA_DEST})
    endif()
endforeach()
install(DIRECTORY ${CRYPTOOL_DEV_DATA_DIR}/PSE DESTINATION ${CRYPTOOL_DATA_DEST} OPTIONAL)

if(APPLE)
    # Copies the Homebrew dylibs (wxWidgets, OpenSSL, GMP) into the bundle so it runs on other Macs.
    install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/CrypTool.app\" \"\" \"\")
    ")
endif()

set(CPACK_PACKAGE_NAME CrypTool)
set(CPACK_PACKAGE_VENDOR "CrypTool project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CrypTool 1 - learn and try out cryptography and cryptanalysis")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE ${REPO_ROOT}/COPYING)
if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME "CrypTool ${PROJECT_VERSION}")
else()
    set(CPACK_GENERATOR TGZ DEB)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "CrypTool project")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION education)
endif()
include(CPack)
