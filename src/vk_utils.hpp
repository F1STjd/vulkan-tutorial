#pragma once

#include <algorithm>
#include <cstring>
#include <expected>
#include <format>
#include <string>

#include <vulkan/vulkan.hpp>

namespace vk_utils
{
/* template<typename ResultValueType, typename MsgFunc>
auto
check_vk_result(ResultValueType result_value, MsgFunc msg_func)
  -> std::expected<std::remove_reference_t<decltype(result_value.value)>,
    std::string>
{
  using ValueType = std::remove_reference_t<decltype(result_value.value)>;
  if (result_value.result != vk::Result::eSuccess)
  {
    return std::expected<ValueType, std::string> { std::unexpect,
      msg_func(result_value.result) };
  }
  return std::move(result_value.value);
} */

template<typename ResultValueType>
auto
check_vk_result(ResultValueType result_value, std::string_view message)
  -> std::expected<std::remove_reference_t<decltype(result_value.value)>,
    std::string>
{
  using ValueType = std::remove_reference_t<decltype(result_value.value)>;
  if (result_value.result != vk::Result::eSuccess)
  {
    auto result_str = vk::to_string(result_value.result);
    return std::expected<ValueType, std::string> {
      std::unexpect,
      std::vformat(message, std::make_format_args(result_str)),
    };
  }
  return std::move(result_value.value);
}

template<typename Properties, typename Projection>
auto
validate_required(std::vector<const char*> required,
  const Properties& available, Projection proj, std::string_view error_prefix)
  -> std::expected<std::vector<const char*>, std::string>
{
  for (const auto* required_item : required)
  {
    if (std::ranges::none_of(available,
          [ required_item, &proj ](const auto& property) noexcept -> bool
          { return std::strcmp(proj(property), required_item) == 0; }))
    {
      return std::expected<std::vector<const char*>, std::string> {
        std::unexpect, std::format("{}: {}", error_prefix, required_item)
      };
    }
  }
  return required;
}
} // namespace vk_utils