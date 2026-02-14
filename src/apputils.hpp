#pragma once

#include <cstdint>
#include <string_view>

namespace apputils
{
enum class error : std::uint8_t
{
  glfw_surface_creation_failed,
  no_suitable_gpu,
  missing_queue_families,
  shader_file_not_found,
  missing_required_layer,
  missing_required_extension,
  wait_for_fences_failed,
  queue_present_failed
};

constexpr auto
to_string(error e) -> std::string_view
{
  switch (e)
  {
  case error::glfw_surface_creation_failed:
    return "GLFW surface creation failed";
  case error::no_suitable_gpu:
    return "no suitable GPU found";
  case error::missing_queue_families:
    return "required queue families not available";
  case error::shader_file_not_found:
    return "shader file not found";
  case error::missing_required_layer:
    return "required Vulkan layer not available";
  case error::missing_required_extension:
    return "required Vulkan extension not available";
  case error::wait_for_fences_failed:
    return "failed to wait for fence";
  case error::queue_present_failed:
    return "queue failed to present image";
  }
  return "unknown error";
}

} // namespace apputils