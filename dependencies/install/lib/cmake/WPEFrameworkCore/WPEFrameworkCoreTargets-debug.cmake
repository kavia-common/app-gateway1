#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkCore::WPEFrameworkCore" for configuration "Debug"
set_property(TARGET WPEFrameworkCore::WPEFrameworkCore APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(WPEFrameworkCore::WPEFrameworkCore PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libWPEFrameworkCore.so.1.0.0"
  IMPORTED_SONAME_DEBUG "libWPEFrameworkCore.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkCore::WPEFrameworkCore )
list(APPEND _cmake_import_check_files_for_WPEFrameworkCore::WPEFrameworkCore "${_IMPORT_PREFIX}/lib/libWPEFrameworkCore.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
