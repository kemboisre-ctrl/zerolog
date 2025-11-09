#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "zerolog::zerolog" for configuration "Release"
set_property(TARGET zerolog::zerolog APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(zerolog::zerolog PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libzerolog.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS zerolog::zerolog )
list(APPEND _IMPORT_CHECK_FILES_FOR_zerolog::zerolog "${_IMPORT_PREFIX}/lib/libzerolog.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
