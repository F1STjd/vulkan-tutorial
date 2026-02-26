cmake_minimum_required(VERSION 4.2)

#set(CMAKE_CXX_STANDARD 26)
#set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
#set(CMAKE_CXX_MODULE_STD 1)
#set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(vulkan-tutorial)

# Enable clang-tidy if available
#find_program(
#  CLANG_TIDY_EXE   
#  NAMES "clang-tidy"
#)

#if(CLANG_TIDY_EXE)
#  get_filename_component(PROJECT_SOURCE_DIR_ABS "${CMAKE_SOURCE_DIR}" ABSOLUTE)

#  set(CMAKE_CXX_CLANG_TIDY 
#    "${CLANG_TIDY_EXE}"
#    "--extra-arg-before=--driver-mode=g++"
#    "--header-filter=${PROJECT_SOURCE_DIR_ABS}/(src|include)/.*"
#    "--warnings-as-errors=-*"
#    "--system-headers=false"
#    "--extra-arg=-Wno-unknown-warning-option"
#    "--extra-arg=-Wno-unknown-argument"
#    "--extra-arg=-Qunused-arguments"
#    "--extra-arg=-Wno-error=unknown-warning-option"
#    "--checks=-clang-diagnostic-unknown-argument"
#  )
#endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(tinygltf REQUIRED)
find_package(KTX REQUIRED)

#add_library(VulkanCppModule)
#add_library(Vulkan::cppm ALIAS VulkanCppModule)

#target_include_directories(VulkanCppModule PRIVATE "${Vulkan_INCLUDE_DIR}")

#target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)

#set_target_properties(VulkanCppModule PROPERTIES
#  CXX_STANDARD 26
#  CXX_MODULE_STD ON
#)

#target_sources(VulkanCppModule
#  PUBLIC
#  FILE_SET cxx_modules TYPE CXX_MODULES
#  BASE_DIRS "${Vulkan_INCLUDE_DIR}"
#  FILES
#    "${Vulkan_INCLUDE_DIR}/vulkan/vulkan_video.cppm"
#    "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
#)

add_executable(main main.cpp src/implementations.cpp)

# Suppress all warnings for third-party STB implementation
set_source_files_properties(src/stb_image_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")

target_compile_definitions(
  main PUBLIC
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
  SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
  TEXTURES_DIR="${CMAKE_SOURCE_DIR}/textures"
)

target_link_libraries(
  main PRIVATE
  Vulkan::Vulkan
#  Vulkan::cppm
  glfw
  glm::glm
  tinyobjloader::tinyobjloader
  tinygltf::tinygltf  
  KTX::ktx
)

target_compile_features(main PUBLIC cxx_std_26)

# GCC warning flags (optimization level comes from preset)
target_compile_options(main PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:
    -Wpedantic
    -pedantic-errors
    -Wall
    -Wextra
    -Wfatal-errors
    -Wabi
    -Wdouble-promotion
    -Wnoexcept
    -Wdeprecated
    -Wctor-dtor-privacy
    -Wredundant-tags
    -Weffc++
    -Wstrict-null-sentinel
    -Wold-style-cast
    -Wsign-promo
    -Wmismatched-tags
    -Wextra-semi
    -Wsuggest-final-types
    -Wsuggest-final-methods
    -Wsuggest-override
    -Wnull-dereference
    -Wnrvo
    -Wuseless-cast
    -Wuninitialized
    -Wstrict-overflow=4
    -Wsuggest-attribute=pure
    -Wsuggest-attribute=const
    -Wsuggest-attribute=noreturn
    -Wmissing-noreturn
    -Wsuggest-attribute=malloc
    -Wsuggest-attribute=returns_nonnull
    -Wsuggest-attribute=format
    -Wmissing-format-attribute
    -Wsuggest-attribute=cold
    -Walloc-size
    -Walloc-zero
    -Warith-conversion
    -Wduplicated-branches
    -Wduplicated-cond
    -Wzero-as-null-pointer-constant
    -Wtrampolines
    -Wshadow
    -Wunsafe-loop-optimizations
    -Wcast-qual
    -Wcast-align
    -Wconversion
    -Wenum-conversion
    -Wlogical-op
    -Wno-pedantic-ms-format
    #-fanalyzer
    -fmessage-length=100
    -fdiagnostics-color
    #-fdiagnostics-urls=always
    -fdiagnostics-parseable-fixits
    #-fdiagnostics-generate-patch
    #-Werror
    -Wno-error=analyzer-symbol-too-complex
    -Wno-error=analyzer-too-complex
    -Wno-error=analyzer-exposure-through-uninit-copy
  >
)

# Link stdc++exp for GCC experimental features (std::println) on Windows
target_link_libraries(main PRIVATE
  $<$<AND:$<CXX_COMPILER_ID:GNU>,$<PLATFORM_ID:Windows>>:stdc++exp>
)

target_include_directories(main SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/external")
target_include_directories(main SYSTEM PRIVATE "D:/msys2/mingw64/include")
target_include_directories(main PRIVATE "${CMAKE_SOURCE_DIR}/src")

set_target_properties(main PROPERTIES
  CXX_SCAN_FOR_MODULES OFF
)

# Precompiled header — all third-party and std headers with defines baked in
target_precompile_headers(main PRIVATE src/pch.hpp)

add_subdirectory(src)
add_subdirectory(lib)
add_subdirectory(shaders)