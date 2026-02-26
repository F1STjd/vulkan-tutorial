#pragma once

#include <array>

#include <glm/glm.hpp>

#include <vulkan/vulkan.hpp>

struct vertex
{
  glm::vec3 position;
  glm::vec3 color;
  glm::vec2 texture_coordinates;

  constexpr auto
  operator==(const vertex& other) const -> bool = default;

  static consteval auto
  get_binding_description() -> vk::VertexInputBindingDescription
  {
    return {
      .binding = 0,
      .stride = sizeof(vertex),
      .inputRate = vk::VertexInputRate::eVertex,
    };
  }

  static consteval auto
  get_attribute_descriptions()
    -> std::array<vk::VertexInputAttributeDescription, 3>
  {
    return {
      vk::VertexInputAttributeDescription {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(vertex, position),
      },
      vk::VertexInputAttributeDescription {
        .location = 1,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(vertex, color),
      },
      vk::VertexInputAttributeDescription {
        .location = 2,
        .binding = 0,
        .format = vk::Format::eR32G32Sfloat,
        .offset = offsetof(vertex, texture_coordinates),
      },
    };
  }
};

namespace std
{
template<>
struct hash<vertex>
{
  constexpr auto
  operator()(const vertex& v) const noexcept -> size_t
  {
    return //
      ((hash<glm::vec3>()(v.position) ^ (hash<glm::vec3>()(v.color) << 1)) >>
        1) ^
      (hash<glm::vec2>()(v.texture_coordinates) << 1);
  }
};
} // namespace std