add_library(load INTERFACE)

target_sources(load
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_SOURCE_DIR}/lib/load"
    FILES
      load.hpp
)

target_link_libraries(load
  INTERFACE
    Vulkan::Vulkan
    tinyobjloader::tinyobjloader
)

target_compile_features(load INTERFACE cxx_std_26)