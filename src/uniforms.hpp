#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>

// https://docs.vulkan.org/spec/latest/chapters/interfaces.html#interfaces-resources-layout
// TODO(Konrad): Should do rest
static constexpr auto mat4_alignment { 16 };

struct uniform_buffer_object
{
  alignas(mat4_alignment) glm::mat4 model;
  alignas(mat4_alignment) glm::mat4 view;
  alignas(mat4_alignment) glm::mat4 proj;
};