#pragma once

#include <array>

#include <glm/glm.hpp>

#include <vulkan/vulkan.hpp>

struct vertex
{
  glm::vec2 position;
  glm::vec3 color;
  glm::vec2 texture_coordinates;

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
        .format = vk::Format::eR32G32Sfloat,
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

static constexpr std::array example_vertices {
  vertex {
    .position { -0.5F, -0.5F },
    .color { 1.0F, 0.0F, 0.0F },
    .texture_coordinates { 0.0F, 1.0F },
  },
  vertex {
    .position { 0.5F, -0.5F },
    .color { 0.0F, 1.0F, 0.0F },
    .texture_coordinates { 1.0F, 1.0F },
  },
  vertex {
    .position { 0.5F, 0.5F },
    .color { 0.0F, 0.0F, 1.0F },
    .texture_coordinates { 1.0F, 0.0F },
  },
  vertex {
    .position { -0.5F, 0.5F },
    .color { 1.0F, 1.0F, 1.0F },
    .texture_coordinates { 0.0F, 0.0F },
  },
};

static constexpr std::array example_indices { 0U, 1U, 2U, 2U, 3U, 0U };