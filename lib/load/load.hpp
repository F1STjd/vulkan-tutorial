#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <vector>

#include "apputils.hpp"
#include "vertex.hpp"

namespace load
{
constexpr auto
shader(const std::filesystem::path& path)
  -> std::expected<std::vector<char>, apputils::error>
{
  std::ifstream input_file {
    path,
    std::ios::ate | std::ios::binary,
  };

  if (!input_file.is_open())
  {
    return std::expected<std::vector<char>, apputils::error> {
      std::unexpect,
      apputils::error::shader_file_not_found,
    };
  }

  std::vector<char> buffer(static_cast<std::size_t>(input_file.tellg()));
  input_file.seekg(0, std::ios::beg);
  input_file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

  return { buffer };
}

constexpr auto
model_obj(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices,
  const char* model_path) -> std::expected<void, apputils::error>
{
  tinyobj::attrib_t attributes;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warnings;
  std::string errors;

  if (!tinyobj::LoadObj(
        &attributes, &shapes, &materials, &warnings, &errors, model_path))
  {
    return std::unexpected { apputils::error::tinyobj_load_failed };
  }

  // TODO(Konrad): any algorith that replaces it?
  for (const auto& shape : shapes)
  {
    for (const auto& index : shape.mesh.indices)
    {
      vertex v {};

      v.position = {
        attributes
          .vertices[ (3UZ * static_cast<std::size_t>(index.vertex_index)) + 0 ],
        attributes
          .vertices[ (3UZ * static_cast<std::size_t>(index.vertex_index)) + 1 ],
        attributes
          .vertices[ (3UZ * static_cast<std::size_t>(index.vertex_index)) + 2 ],
      };

      v.texture_coordinates = {
        attributes
          .texcoords[ (2UZ * static_cast<std::size_t>(index.texcoord_index)) +
            0 ],
        1.0F -
          attributes
            .texcoords[ (2UZ * static_cast<std::size_t>(index.texcoord_index)) +
              1 ],
      };

      v.color = { 1.0F, 1.0F, 1.0F };

      vertices.push_back(v);
      indices.push_back(static_cast<std::uint32_t>(indices.size()));
    }
  }

  return {};
}
} // namespace load