cmake_minimum_required(VERSION 3.20)

project(CrypTool1 VERSION 1.4.31 LANGUAGES C CXX)

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
add_subdirectory(port/app)
