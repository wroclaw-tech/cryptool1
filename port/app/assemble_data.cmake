# Builds the data directory the application reads at run time (a development stand-in for the
# installed resources): links to the bitmaps and template files plus freshly generated key stores.
# The help directories are set up by the cryptool_help target (port/tools/build_help_index.py).
#   cmake -DREPO_ROOT=... -DDATA_DIR=... -DMKPSE=... -P assemble_data.cmake

file(MAKE_DIRECTORY "${DATA_DIR}")

function(link_into name target)
    if(NOT EXISTS "${DATA_DIR}/${name}" AND EXISTS "${target}")
        file(CREATE_LINK "${target}" "${DATA_DIR}/${name}" SYMBOLIC)
    endif()
endfunction()

link_into(res "${REPO_ROOT}/CrypTool/res")

file(GLOB template_entries RELATIVE "${REPO_ROOT}/setup/template" "${REPO_ROOT}/setup/template/*")
foreach(entry IN LISTS template_entries)
    string(TOLOWER "${entry}" lower)
    if(lower MATCHES "\\.(dll|manifest)$" OR lower STREQUAL "pse")
        continue()
    endif()
    link_into("${entry}" "${REPO_ROOT}/setup/template/${entry}")
endforeach()

if(MKPSE AND NOT EXISTS "${DATA_DIR}/PSE/PSECA/capse.cse")
    execute_process(COMMAND "${MKPSE}" "${DATA_DIR}" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(WARNING "secude_compat_mkpse failed: ${result}")
    endif()
endif()
