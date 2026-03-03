#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <unordered_map>
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

/* constexpr auto
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
    return std::unexpected { apputils::error::model_load_failed };
  }

  std::unordered_map<vertex, std::uint32_t> unique_vertices;

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

      if (!unique_vertices.contains(v))
      {
        unique_vertices[ v ] = static_cast<std::uint32_t>(vertices.size());
        vertices.push_back(v);
      }
      indices.push_back(unique_vertices[ v ]);
    }
  }

  return {};
} */

// TODO(Konrad): Too big function. Before fixing try fastgltf api
constexpr auto
model_gltf(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices,
  const std::string& model_path) -> std::expected<void, apputils::error>
{
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string errors;
  std::string warnings;

  const bool loaded =
    loader.LoadBinaryFromFile(&model, &errors, &warnings, model_path);
  if (!warnings.empty()) { std::println("glTF warnings: {}", warnings); }
  if (!errors.empty()) { std::println("glTF errors: {}", warnings); }
  if (!loaded)
  {
    return std::unexpected { apputils::error::model_load_failed };
  }

  std::unordered_map<vertex, std::uint32_t> unique_vertices;

  for (const auto& mesh : model.meshes)
  {
    for (const auto& primitive : mesh.primitives)
    {
      const auto indices_i = static_cast<std::size_t>(primitive.indices);
      const auto& index_accessor = model.accessors[ indices_i ];
      const auto index_buffer_view_i =
        static_cast<std::size_t>(index_accessor.bufferView);
      const auto& index_buffer_view = model.bufferViews[ index_buffer_view_i ];
      const auto index_buffer_i =
        static_cast<std::size_t>(index_buffer_view.buffer);
      const auto& index_buffer = model.buffers[ index_buffer_i ];

      const auto position_i =
        static_cast<std::size_t>(primitive.attributes.at("POSITION"));
      const auto& position_accessor = model.accessors[ position_i ];
      const auto position_buffer_view_i =
        static_cast<std::size_t>(position_accessor.bufferView);
      const auto& position_buffer_view =
        model.bufferViews[ position_buffer_view_i ];
      const auto position_buffer_i =
        static_cast<std::size_t>(position_buffer_view.buffer);
      const auto& position_buffer = model.buffers[ position_buffer_i ];

      const bool has_texture_coords =
        primitive.attributes.contains("TEXCOORD_0");
      const tinygltf::Accessor* texture_coords_accessor { nullptr };
      const tinygltf::BufferView* texture_coords_buffer_view { nullptr };
      const tinygltf::Buffer* texture_coords_buffer { nullptr };
      if (has_texture_coords)
      {
        const auto texture_coords_i =
          static_cast<std::size_t>(primitive.attributes.at("TEXCOORD_0"));
        texture_coords_accessor = &model.accessors[ texture_coords_i ];
        const auto texture_coords_buffer_view_i =
          static_cast<std::size_t>(texture_coords_accessor->bufferView);
        texture_coords_buffer_view =
          &model.bufferViews[ texture_coords_buffer_view_i ];
        const auto texture_coords_buffer_i =
          static_cast<std::size_t>(texture_coords_buffer_view->buffer);
        texture_coords_buffer = &model.buffers[ texture_coords_buffer_i ];
      }

      const auto base_vertex = static_cast<std::uint32_t>(vertices.size());

      for (auto i : std::views::iota(0UZ, position_accessor.count))
      {
        vertex v {};

        const auto* position = reinterpret_cast<const float*>(
          &position_buffer.data[ position_buffer_view.byteOffset +
            position_accessor.byteOffset + (i * sizeof(vertex::position)) ]);
        const std::span<const float, 3> positions { position, 3 };
        v.position = { positions[ 0 ], -positions[ 1 ], positions[ 2 ] };

        if (has_texture_coords)
        {
          const auto* texture_coord =
            reinterpret_cast<const float*>(&texture_coords_buffer
                ->data[ texture_coords_buffer_view->byteOffset +
                  texture_coords_accessor->byteOffset +
                  (i * sizeof(vertex::texture_coordinates)) ]);
          const std::span<const float, 2> texture_coords { texture_coord, 2 };
          v.texture_coordinates = { texture_coords[ 0 ], texture_coords[ 1 ] };
        }
        else
        {
          v.texture_coordinates = { 0.0F, 0.0F };
        }

        v.color = { 1.0F, 1.0F, 1.0F };

        vertices.push_back(v);
      }

      const unsigned char* index_data =
        &index_buffer
           .data[ index_buffer_view.byteOffset + index_accessor.byteOffset ];
      std::size_t index_count { index_accessor.count };
      std::size_t index_stride {};

      if (index_accessor.componentType ==
        TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
      {
        index_stride = sizeof(std::uint16_t);
      }
      else if (index_accessor.componentType ==
        TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
      {
        index_stride = sizeof(std::uint32_t);
      }
      else if (index_accessor.componentType ==
        TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
      {
        index_stride = sizeof(std::uint8_t);
      }
      else
      {
        return std::unexpected {
          apputils::error::unsupported_index_component_type,
        };
      }

      indices.reserve(indices.size() + index_count);

      for (auto i : std::views::iota(0UZ, index_count))
      {
        std::uint32_t resulting_index {};

        if (index_accessor.componentType ==
          TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
          resulting_index = *reinterpret_cast<const std::uint16_t*>(
            index_data + (i * index_stride));
        }
        else if (index_accessor.componentType ==
          TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
          resulting_index = *reinterpret_cast<const std::uint32_t*>(
            index_data + (i * index_stride));
        }
        else if (index_accessor.componentType ==
          TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        {
          resulting_index = *(index_data + (i * index_stride));
        }

        indices.push_back(base_vertex + resulting_index);
      }
    }
  }

  return {};
}
} // namespace load