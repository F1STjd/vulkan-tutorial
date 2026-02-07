# Findtinyobjloader.cmake
# Find the tinyobjloader library (header-only)
#
# This module defines:
#   tinyobjloader_FOUND        - True if tinyobjloader was found
#   tinyobjloader_INCLUDE_DIRS - Include directories for tinyobjloader
#
# Imported targets:
#   tinyobjloader::tinyobjloader

find_path(tinyobjloader_INCLUDE_DIR
  NAMES tiny_obj_loader.h
  PATHS
    ${CMAKE_SOURCE_DIR}/external/tinyobjloader
    ${CMAKE_SOURCE_DIR}/external
    /usr/include
    /usr/local/include
  PATH_SUFFIXES tinyobjloader
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tinyobjloader
  REQUIRED_VARS tinyobjloader_INCLUDE_DIR
)

if(tinyobjloader_FOUND)
  set(tinyobjloader_INCLUDE_DIRS ${tinyobjloader_INCLUDE_DIR})

  if(NOT TARGET tinyobjloader::tinyobjloader)
    add_library(tinyobjloader::tinyobjloader INTERFACE IMPORTED)
    set_target_properties(tinyobjloader::tinyobjloader PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${tinyobjloader_INCLUDE_DIRS}"
    )
  endif()
endif()

mark_as_advanced(tinyobjloader_INCLUDE_DIR)
