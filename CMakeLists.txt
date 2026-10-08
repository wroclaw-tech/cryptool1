cmake_minimum_required(VERSION 3.20)

project(CrypTool1 VERSION 1.4.43 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
enable_testing()

if(APPLE)
    foreach(prefix /opt/homebrew/opt /usr/local/opt)
        if(EXISTS ${prefix}/wxwidgets@3.2/bin/wx-config-3.2 AND NOT wxWidgets_CONFIG_EXECUTABLE)
            set(wxWidgets_CONFIG_EXECUTABLE ${prefix}/wxwidgets@3.2/bin/wx-config-3.2)
        endif()
        if(EXISTS ${prefix}/openssl@3 AND NOT OPENSSL_ROOT_DIR)
            set(OPENSSL_ROOT_DIR ${prefix}/openssl@3)
        endif()
    endforeach()
endif()

find_package(wxWidgets 3.2 REQUIRED COMPONENTS stc aui html gl adv xml core base)
find_package(OpenSSL 3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(port/thirdparty)
add_subdirectory(port/secude)
if(TARGET secude_compat)
    # The application is C++14; the library may use a newer standard internally.
    set_property(TARGET secude_compat PROPERTY INTERFACE_COMPILE_FEATURES "")
endif()
add_subdirectory(port/mfcwx)
# These libraries open files by paths CrypTool passes in Windows form; the app links mfcwx, which
# converts them, and the library tests use plain stubs.
foreach(target cryptool_analyse cryptool_cracklib)
    target_compile_options(${target} PRIVATE "SHELL:-iquote ${CMAKE_SOURCE_DIR}/port/mfcwx/include"
                           "SHELL:-include ${CMAKE_SOURCE_DIR}/port/mfcwx/include/mfcwx/pathcompat.h")
endforeach()
add_library(mfcwx_path_stubs STATIC port/mfcwx/tests/path_stubs.cpp)
target_include_directories(mfcwx_path_stubs PRIVATE port/mfcwx/include)
foreach(test test_analyse test_cracklib)
    if(TARGET ${test})
        target_link_libraries(${test} PRIVATE mfcwx_path_stubs)
    endif()
endforeach()
add_subdirectory(port/app)

option(CRYPTOOL_DIALOG_GALLERY "Build the dialog snapshot tool" OFF)
if(CRYPTOOL_DIALOG_GALLERY)
    add_subdirectory(port/tools/gallery)
endif()
