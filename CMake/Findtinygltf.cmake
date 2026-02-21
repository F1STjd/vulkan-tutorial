# Findtinygltf.cmake
# Find the tinygltf library (header-only)
#
# This module defines:
#   tinygltf_FOUND        - True if tinygltf was found
#   tinygltf_INCLUDE_DIRS - Include directories for tinygltf
#
# Imported targets:
#   tinygltf::tinygltf

find_path(tinygltf_INCLUDE_DIR
  NAMES tiny_gltf.h
  PATHS
    ${CMAKE_SOURCE_DIR}/external/tinygltf
    ${CMAKE_SOURCE_DIR}/external
    /usr/include
    /usr/local/include
  PATH_SUFFIXES tinygltf
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tinygltf
  REQUIRED_VARS tinygltf_INCLUDE_DIR
)

if(tinygltf_FOUND)
  set(tinygltf_INCLUDE_DIRS ${tinygltf_INCLUDE_DIR})

  if(NOT TARGET tinygltf::tinygltf)
    add_library(tinygltf::tinygltf INTERFACE IMPORTED)
    set_target_properties(tinygltf::tinygltf PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${tinygltf_INCLUDE_DIRS}"
      INTERFACE_INCLUDE_DIRECTORIES "${tinygltf_INCLUDE_DIRS}"
    )
    # tinygltf requires these defines when used as header-only
    target_compile_definitions(tinygltf::tinygltf INTERFACE
      TINYGLTF_IMPLEMENTATION
      TINYGLTF_NO_EXTERNAL_IMAGE
      TINYGLTF_NO_STB_IMAGE
      TINYGLTF_NO_STB_IMAGE_WRITE
    )
  endif()
endif()

mark_as_advanced(tinygltf_INCLUDE_DIR)
