include_guard(GLOBAL)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(MFCWX_RC2CPP_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/rc2cpp.py")
get_filename_component(MFCWX_RC_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../mfcwx/include" ABSOLUTE)

# mfcwx_add_rc(<target> RC <rc> [MODULE <name>] [HEADERS <h>...] [OUTPUT_DIR <dir>] [INCLUDE_DIRS <dir>...] [DEFINES <d>...] [CODEPAGE <cp>])
function(mfcwx_add_rc target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "RC;MODULE;OUTPUT_DIR;CODEPAGE" "HEADERS;INCLUDE_DIRS;DEFINES")
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "mfcwx_add_rc: unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT ARG_RC)
    message(FATAL_ERROR "mfcwx_add_rc: RC <file.rc> is required")
  endif()

  get_filename_component(rc "${ARG_RC}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  get_filename_component(rc_dir "${rc}" DIRECTORY)
  get_filename_component(rc_name "${rc}" NAME)
  if(NOT ARG_MODULE)
    get_filename_component(ARG_MODULE "${rc}" NAME_WE)
  endif()
  string(MAKE_C_IDENTIFIER "${ARG_MODULE}" module)
  if(NOT ARG_OUTPUT_DIR)
    set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/rc2cpp/${module}")
  endif()
  get_filename_component(out_dir "${ARG_OUTPUT_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

  set(headers)
  foreach(h IN LISTS ARG_HEADERS)
    get_filename_component(h "${h}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND headers "${h}")
  endforeach()

  set(args "${rc}" --module "${module}" --out-dir "${out_dir}" -I "${MFCWX_RC_INCLUDE_DIR}")
  foreach(d IN LISTS ARG_INCLUDE_DIRS)
    get_filename_component(d "${d}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND args -I "${d}")
  endforeach()
  foreach(d IN LISTS ARG_DEFINES)
    list(APPEND args -D "${d}")
  endforeach()
  if(ARG_CODEPAGE)
    list(APPEND args --codepage "${ARG_CODEPAGE}")
  endif()

  # Output names depend on the language set; the build rewrites outputs_file when it changes, which re-runs CMake.
  set(outputs_file "${out_dir}/${module}_outputs.txt")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${MFCWX_RC2CPP_SCRIPT}" ${args} --list-outputs --outputs-file "${outputs_file}"
    OUTPUT_VARIABLE listed
    ERROR_VARIABLE errors
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "mfcwx_add_rc: rc2cpp.py failed for ${rc}:\n${errors}")
  endif()
  string(STRIP "${listed}" listed)
  string(REPLACE "\n" ";" listed "${listed}")
  set(sources ${listed})
  list(FILTER sources INCLUDE REGEX "\\.cpp$")
  set(files_list "${out_dir}/${module}_files.txt")

  set(depfile_args)
  set(depfile_cli)
  if(CMAKE_GENERATOR MATCHES "Ninja")
    set(depfile "${out_dir}/${module}.d")
    set(depfile_args DEPFILE "${depfile}")
    set(depfile_cli --depfile "${depfile}")
  endif()

  add_custom_command(
    OUTPUT ${sources} "${files_list}"
    COMMAND "${Python3_EXECUTABLE}" "${MFCWX_RC2CPP_SCRIPT}" ${args} --quiet --outputs-file "${outputs_file}" ${depfile_cli}
    DEPENDS
      "${rc}" ${headers} "${MFCWX_RC2CPP_SCRIPT}"
      "${MFCWX_RC_INCLUDE_DIR}/mfcwx/winconst.h" "${MFCWX_RC_INCLUDE_DIR}/afxres.h"
    ${depfile_args}
    COMMENT "rc2cpp: generating resource tables for ${module} from ${rc_name}"
    VERBATIM)

  target_sources(${target} PRIVATE ${sources})
  set_source_files_properties(${sources} PROPERTIES GENERATED TRUE)
  set_property(SOURCE ${sources} APPEND PROPERTY INCLUDE_DIRECTORIES "${MFCWX_RC_INCLUDE_DIR}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${outputs_file}")
  set_property(TARGET ${target} APPEND PROPERTY MFCWX_RC_FILE_LISTS "${files_list}")
  set_property(TARGET ${target} APPEND PROPERTY MFCWX_RC_BASE_DIRS "${rc_dir}")

  set(MFCWX_RC_${module}_SOURCES "${sources}" PARENT_SCOPE)
  set(MFCWX_RC_${module}_FILES_LIST "${files_list}" PARENT_SCOPE)
endfunction()

# mfcwx_install_rc_files(<target> DESTINATION <dir> [COMPONENT <name>]): installs the files referenced by the target's .rc files.
function(mfcwx_install_rc_files target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "DESTINATION;COMPONENT" "")
  if(NOT ARG_DESTINATION)
    message(FATAL_ERROR "mfcwx_install_rc_files: DESTINATION is required")
  endif()
  get_property(lists TARGET ${target} PROPERTY MFCWX_RC_FILE_LISTS)
  get_property(dirs TARGET ${target} PROPERTY MFCWX_RC_BASE_DIRS)
  if(IS_ABSOLUTE "${ARG_DESTINATION}")
    set(dest "${ARG_DESTINATION}")
  else()
    set(dest "\${CMAKE_INSTALL_PREFIX}/${ARG_DESTINATION}")
  endif()
  set(component)
  if(ARG_COMPONENT)
    set(component COMPONENT "${ARG_COMPONENT}")
  endif()
  list(LENGTH lists count)
  if(count EQUAL 0)
    return()
  endif()
  math(EXPR last "${count} - 1")
  foreach(i RANGE ${last})
    list(GET lists ${i} list_file)
    list(GET dirs ${i} base_dir)
    install(CODE "
      file(STRINGS \"${list_file}\" _mfcwx_rc_files)
      foreach(_mfcwx_rc_file IN LISTS _mfcwx_rc_files)
        get_filename_component(_mfcwx_rc_dir \"\${_mfcwx_rc_file}\" DIRECTORY)
        file(INSTALL DESTINATION \"${dest}/\${_mfcwx_rc_dir}\" TYPE FILE FILES \"${base_dir}/\${_mfcwx_rc_file}\")
      endforeach()"
      ${component})
  endforeach()
endfunction()
