#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "gbui::gbui" for configuration ""
set_property(TARGET gbui::gbui APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(gbui::gbui PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libgbui.a"
  )

list(APPEND _cmake_import_check_targets gbui::gbui )
list(APPEND _cmake_import_check_files_for_gbui::gbui "${_IMPORT_PREFIX}/lib64/libgbui.a" )

# Import target "gbui::gbui_meta" for configuration ""
set_property(TARGET gbui::gbui_meta APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(gbui::gbui_meta PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libgbui_meta.a"
  )

list(APPEND _cmake_import_check_targets gbui::gbui_meta )
list(APPEND _cmake_import_check_files_for_gbui::gbui_meta "${_IMPORT_PREFIX}/lib64/libgbui_meta.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
