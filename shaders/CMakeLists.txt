find_program(SLANGC_EXECUTABLE slangc REQUIRED)

function(add_slang_shader_target TARGET)
  cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})
  set(SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR})
  set(ENTRY_POINTS -entry vertex_main -entry fragment_main)
  add_custom_command(
    OUTPUT ${SHADERS_DIR}/slang.spv
    COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SHADER_SOURCES}
    COMMENT "Compiling Slang Shaders"
    VERBATIM
  )
  add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

set(SHADER_SLANG_SOURCES ${CMAKE_CURRENT_LIST_DIR}/shader.slang)

add_slang_shader_target(shaders SOURCES ${SHADER_SLANG_SOURCES})
add_dependencies(main shaders)
