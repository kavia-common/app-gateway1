#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkCOM::WPEFrameworkCOM" for configuration "Debug"
set_property(TARGET WPEFrameworkCOM::WPEFrameworkCOM APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(WPEFrameworkCOM::WPEFrameworkCOM PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "WPEFrameworkCore::WPEFrameworkCore;WPEFrameworkMessaging::WPEFrameworkMessaging"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libWPEFrameworkCOM.so.1.0.0"
  IMPORTED_SONAME_DEBUG "libWPEFrameworkCOM.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkCOM::WPEFrameworkCOM )
list(APPEND _cmake_import_check_files_for_WPEFrameworkCOM::WPEFrameworkCOM "${_IMPORT_PREFIX}/lib/libWPEFrameworkCOM.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
