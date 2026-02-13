#pragma once

#include <algorithm>
#include <expected>
#include <source_location>

#include <variant>
#include <vulkan/vulkan.hpp>

#include "apdevpkey.h"
#include "app_utils.hpp"

namespace vk_utils
{

struct error
{
  std::variant<vk::Result, app_utils::error> reason;
  std::source_location location;

  template<class... Ts>
  struct overloaded : Ts...
  {
    using Ts::operator()...;
  };

  [[nodiscard]] constexpr auto
  to_string() const -> std::string
  {
    return std::visit(
      overloaded {
        [](vk::Result r) -> std::string { return vk::to_string(r); },
        [](app_utils::error e) -> std::string
        { return std::string { app_utils::to_string(e) }; },
      },
      reason);
  }
};

template<typename T>
constexpr auto
locate(std::expected<T, vk::Result> result,
  std::source_location loc = std::source_location::current())
  -> std::expected<T, error>
{
  if (result) { return std::move(*result); }
  return std::unexpected { error { result.error(), loc } };
}

template<typename T>
constexpr auto
locate(std::expected<T, app_utils::error> result,
  std::source_location loc = std::source_location::current())
  -> std::expected<T, error>
{
  if (result) { return std::move(*result); }
  return std::unexpected { error { result.error(), loc } };
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
  const Properties& available, Projection proj)
  -> std::expected<std::vector<const char*>, vk::Result>
{
  for (const auto* required_item : required)
  {
    if (std::ranges::none_of(available,
          [ required_item, &proj ](const auto& property) noexcept -> bool
          { return std::strcmp(proj(property), required_item) == 0; }))
      [[unlikely]]
    {
      return std::unexpected { vk::Result::eErrorUnknown };
    }
  }
  return required;
}
} // namespace vk_utils