file(GLOB MFCWX_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(mfcwx STATIC ${MFCWX_SOURCES})
target_include_directories(mfcwx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mfcwx PUBLIC cxx_std_14)
include(${wxWidgets_USE_FILE})
target_include_directories(mfcwx SYSTEM PUBLIC ${wxWidgets_INCLUDE_DIRS})
target_compile_definitions(mfcwx PUBLIC ${wxWidgets_DEFINITIONS} MFCWX=1)
target_compile_options(mfcwx PUBLIC ${wxWidgets_CXX_FLAGS})
target_link_libraries(mfcwx PUBLIC ${wxWidgets_LIBRARIES} OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})
if(APPLE)
    target_link_libraries(mfcwx PUBLIC "-framework OpenGL")
endif()

option(MFCWX_TESTS "Build the mfcwx runtime tests" OFF)
if(MFCWX_TESTS)
    # The runtime sources are compiled directly so the tests do not depend on the GUI parts;
    # tests/runtime_stubs.cpp provides the few GUI symbols they reference.
    set(MFCWX_RUNTIME_SOURCES
        afxcore.cpp afxfile.cpp afxmt.cpp codepage.cpp codepage_tables.cpp crt.cpp cstring.cpp file.cpp kernel.cpp
        profile.cpp thread.cpp win32.cpp)
    list(TRANSFORM MFCWX_RUNTIME_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)
    add_executable(mfcwx_runtime_tests tests/runtime_tests.cpp tests/runtime_stubs.cpp ${MFCWX_RUNTIME_SOURCES})
    target_include_directories(mfcwx_runtime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                               ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(mfcwx_runtime_tests SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_features(mfcwx_runtime_tests PRIVATE cxx_std_14)
    target_compile_definitions(mfcwx_runtime_tests PRIVATE ${wxWidgets_DEFINITIONS} MFCWX=1)
    target_compile_options(mfcwx_runtime_tests PRIVATE ${wxWidgets_CXX_FLAGS})
    target_link_libraries(mfcwx_runtime_tests PRIVATE ${wxWidgets_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
    add_test(NAME mfcwx_runtime_tests COMMAND mfcwx_runtime_tests)

    # GDI tests: render through CDC into bitmaps, check pixels and write PNGs for inspection.
    set(MFCWX_GDI_SOURCES gdi.cpp gdi_bits.cpp gdi_dc.cpp gdi_text.cpp codepage.cpp codepage_tables.cpp cstring.cpp)
    list(TRANSFORM MFCWX_GDI_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)
    add_executable(mfcwx_gdi_tests tests/gdi_tests.cpp tests/gdi_test_stubs.cpp ${MFCWX_GDI_SOURCES})
    target_include_directories(mfcwx_gdi_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(mfcwx_gdi_tests SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_features(mfcwx_gdi_tests PRIVATE cxx_std_14)
    target_compile_definitions(mfcwx_gdi_tests PRIVATE ${wxWidgets_DEFINITIONS} MFCWX=1)
    target_compile_options(mfcwx_gdi_tests PRIVATE ${wxWidgets_CXX_FLAGS})
    target_link_libraries(mfcwx_gdi_tests PRIVATE ${wxWidgets_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
    add_test(NAME mfcwx_gdi_tests COMMAND mfcwx_gdi_tests ${CMAKE_CURRENT_BINARY_DIR}/gdi_test_output)

    # Scintilla proxy; tests/scintilla_tests.cpp records the notifications it sends.
    add_executable(mfcwx_scintilla_tests tests/scintilla_tests.cpp src/scintilla.cpp src/codepage.cpp
                   src/codepage_tables.cpp)
    target_include_directories(mfcwx_scintilla_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                               ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(mfcwx_scintilla_tests SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_features(mfcwx_scintilla_tests PRIVATE cxx_std_14)
    target_compile_definitions(mfcwx_scintilla_tests PRIVATE ${wxWidgets_DEFINITIONS} MFCWX=1)
    target_compile_options(mfcwx_scintilla_tests PRIVATE ${wxWidgets_CXX_FLAGS})
    target_link_libraries(mfcwx_scintilla_tests PRIVATE ${wxWidgets_LIBRARIES} Threads::Threads)
    add_test(NAME mfcwx_scintilla_tests COMMAND mfcwx_scintilla_tests)
endif()
