include(${REPO_ROOT}/port/rc/rc2cpp.cmake)

add_executable(dialog_gallery gallery.cpp)
mfcwx_add_rc(dialog_gallery RC ${REPO_ROOT}/CrypTool/CrypTool.rc MODULE CrypTool HEADERS ${REPO_ROOT}/CrypTool/resource.h
             REPLACE "[CRYPTOOL_VERSION_FULL]=CrypTool ${PROJECT_VERSION}" "[CRYPTOOL_VERSION]=${PROJECT_VERSION}"
             OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/rc)
target_link_libraries(dialog_gallery PRIVATE mfcwx)
target_compile_definitions(dialog_gallery PRIVATE CRYPTOOL_DEV_DATA_DIR="${CMAKE_BINARY_DIR}/data")
