#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "app_utils.hpp"

namespace load
{
constexpr auto
shader(const std::filesystem::path& path)
  -> std::expected<std::vector<char>, app_utils::error>
{
  std::ifstream input_file {
    path,
    std::ios::ate | std::ios::binary,
  };

  if (!input_file.is_open())
  {
    return std::expected<std::vector<char>, app_utils::error> {
      std::unexpect,
      app_utils::error::shader_file_not_found,
    };
  }

  std::vector<char> buffer(static_cast<std::size_t>(input_file.tellg()));
  input_file.seekg(0, std::ios::beg);
  input_file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

  return { buffer };
}
} // namespace load