#pragma once

#include <algorithm>
#include <cstring>
#include <expected>
#include <format>
#include <string>

#include <vulkan/vulkan.hpp>

namespace vk_utils
{

template<typename ResultValueType>
constexpr auto
check_vk_result(ResultValueType result_value, std::string_view message)
  -> std::expected<std::remove_reference_t<decltype(result_value.value)>,
    std::string>
{
  using ValueType = std::remove_reference_t<decltype(result_value.value)>;
  if (result_value.result != vk::Result::eSuccess) [[unlikely]]
  {
    auto result_str = vk::to_string(result_value.result);
    return std::expected<ValueType, std::string> {
      std::unexpect,
      std::vformat(message, std::make_format_args(result_str)),
    };
  }
  return std::move(result_value.value);
}

template<typename T>
constexpr auto
store_into(T& out)
{
  return [ &out ](auto&& value) noexcept -> void
  { out = std::forward<decltype(value)>(value); };
}

template<typename Properties, typename Projection>
auto constexpr validate_required(std::vector<const char*> required,
  const Properties& available, Projection proj, std::string_view error_prefix)
  -> std::expected<std::vector<const char*>, std::string>
{
  for (const auto* required_item : required)
  {
    if (std::ranges::none_of(available,
          [ required_item, &proj ](const auto& property) noexcept -> bool
          { return std::strcmp(proj(property), required_item) == 0; }))
      [[unlikely]]
    {
      return std::expected<std::vector<const char*>, std::string> {
        std::unexpect, std::format("{}: {}", error_prefix, required_item)
      };
    }
  }
  return required;
}
} // namespace vk_utils