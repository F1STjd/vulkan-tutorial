#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <utility>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#define VULKAN_HPP_NO_EXCEPTIONS
#include "vulkan/vulkan.hpp"
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_utils.hpp>

static constexpr std::uint32_t width { 800 };
static constexpr std::uint32_t height { 600 };

static constexpr std::array validation_layers { "VK_LAYER_KHRONOS_validation" };
static constexpr std::array device_extensions { vk::KHRSwapchainExtensionName };

#ifdef NDEBUG
static constexpr bool enable_validation_layers { false };
#else
static constexpr bool enable_validation_layers { true };
#endif

struct app
{
public:
  void
  run()
  {
    auto init_result = //
      init_window().and_then(
        [ this ] -> std::expected<void, std::string> { return init_vulkan(); });

    if (!init_result.has_value())
    {
      std::println(std::cerr,
        "Failed to initialize application, with error: {}",
        init_result.error());
      std::exit(EXIT_FAILURE);
    }
    main_loop();
    cleanup();
  }

private:
  auto
  init_window() -> std::expected<void, std::string>
  {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

    return {};
  }

  auto
  init_vulkan() -> std::expected<void, std::string>
  {
    return create_instance()
      .and_then([ this ] -> std::expected<void, std::string>
        { return setup_debug_messenger(); })
      .and_then([ this ] -> std::expected<void, std::string>
        { return create_surface(); })
      .and_then([ this ] -> std::expected<void, std::string>
        { return pick_physical_device(); })
      .and_then([ this ] -> std::expected<void, std::string>
        { return create_logical_device(); });
  }

  void
  main_loop()
  {
    while (glfwWindowShouldClose(window_) == 0)
    {
      glfwPollEvents();
    }
  }

  void
  cleanup()
  {
    glfwDestroyWindow(window_);
    glfwTerminate();
  }

  auto
  create_instance() -> std::expected<void, std::string>
  {
    return get_required_layers() //
      .and_then(
        [ this ](auto layers) -> std::expected<void, std::string>
        {
          return get_required_extensions() //
            .and_then([ this, layers = std::move(layers) ](
                        auto extensions) -> std::expected<void, std::string>
              { return create_vulkan_instance(layers, extensions); });
        });
  }

  auto
  setup_debug_messenger() -> std::expected<void, std::string>
  {
    if (!enable_validation_layers) { return {}; }

    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags {
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    };
    vk::DebugUtilsMessageTypeFlagsEXT message_type_flags {
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
    };
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info_EXT {
      .messageSeverity = severity_flags,
      .messageType = message_type_flags,
      .pfnUserCallback = &debug_callback
    };

    return //
      vk_utils::check_vk_result(instance_.createDebugUtilsMessengerEXT(
                                  debug_utils_messenger_create_info_EXT),
        "Failed to create debug messenger, with result: {}")
        .transform([ this ](auto messenger) noexcept -> auto
          { debug_messenger_ = std::move(messenger); });
  }

  auto
  create_surface() -> std::expected<void, std::string>
  {
    VkSurfaceKHR _surface {};
    if (glfwCreateWindowSurface(*instance_, window_, nullptr, &_surface) != 0)
    {
      return std::expected<void, std::string> {
        std::unexpect,
        std::format("Failed to create window surface"),
      };
    }

    surface_ = vk::raii::SurfaceKHR(instance_, _surface);
    return {};
  }

  auto
  pick_physical_device() -> std::expected<void, std::string>
  {
    return //
      vk_utils::check_vk_result(instance_.enumeratePhysicalDevices(),
        "Failed to enumerate physical devices: {}")
        .and_then(
          [ this ](const auto& devices) -> std::expected<void, std::string>
          {
            for (const auto& device : devices)
            {
              if (is_device_suitable(device))
              {
                physical_device_ = device;
                return {};
              }
            }
            return std::expected<void, std::string> { std::unexpect,
              "Failed to find suitable GPU" };
          });
  }

  auto
  create_logical_device() -> std::expected<void, std::string>
  {
    auto [ graphics_index, presentation_index ] = get_queue_family_indices();

    if (!graphics_index || !presentation_index)
    {
      return std::expected<void, std::string> {
        std::unexpect,
        "Could not find required queue families",
      };
    }

    // tutorial says it could be used later (if not I'll remove it)
    [[maybe_unused]] vk::PhysicalDeviceFeatures device_features;

    vk::StructureChain feature_chain {
      vk::PhysicalDeviceFeatures2 {},
      vk::PhysicalDeviceVulkan13Features {
        .dynamicRendering = vk::Bool32 { true },
      },
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT {
        .extendedDynamicState = vk::Bool32 { true },
      },
    };

    constexpr auto queue_priority { 0.5F };
    vk::DeviceQueueCreateInfo device_queue_create_info {
      .queueFamilyIndex = *graphics_index,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
    };

    vk::DeviceCreateInfo device_create_info {
      .pNext = &feature_chain.get(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &device_queue_create_info,
      .enabledExtensionCount =
        static_cast<std::uint32_t>(device_extensions.size()),
      .ppEnabledExtensionNames = device_extensions.data(),
    };

    return vk_utils::check_vk_result(
      physical_device_.createDevice(device_create_info),
      "Failed to create logical device, with result: {}")
      .transform(
        [ this, graphics_index ](auto device) noexcept -> auto
        {
          device_ = std::move(device);
          graphics_queue_ = device_.getQueue(*graphics_index, 0);
        });
  }

  auto
  get_required_layers() -> std::expected<std::vector<const char*>, std::string>
  {
    std::vector<const char*> required_layers;
    if constexpr (enable_validation_layers)
    {
      required_layers.assign_range(validation_layers);
    }

    return //
      vk_utils::check_vk_result(context_.enumerateInstanceLayerProperties(),
        "Failed to enumerate instance layer properties, with result: {} ")
        .and_then(
          [ required_layers = std::move(required_layers) ](
            const auto& layer_properties) -> auto
          {
            return vk_utils::validate_required(
              std::move(required_layers), layer_properties,
              [](const auto& prop) noexcept -> auto
              { return prop.layerName.data(); },
              "Required layer is not supported");
          });
  }

  auto
  get_required_extensions()
    -> std::expected<std::vector<const char*>, std::string>
  {
    std::uint32_t glfw_extension_count {};
    auto* glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> required_extensions;
    required_extensions.reserve(glfw_extension_count + 1U);
    for (const auto* extension :
      std::span { glfw_extensions, glfw_extension_count })
    {
      required_extensions.push_back(extension);
    }

    if constexpr (enable_validation_layers)
    {
      required_extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return //
      vk_utils::check_vk_result(context_.enumerateInstanceExtensionProperties(),
        "Failed to enumerate instance extension properties, with result: {}")
        .and_then(
          [ required_extensions = std::move(required_extensions) ](
            const auto& extension_properties) -> auto
          {
            return vk_utils::validate_required(
              std::move(required_extensions), extension_properties,
              [](const auto& prop) noexcept -> auto
              { return prop.extensionName.data(); },
              "Required extension is not supported");
          });
  }

  auto
  create_vulkan_instance(std::span<const char* const> layers,
    std::span<const char* const> extensions) -> std::expected<void, std::string>
  {
    constexpr vk::ApplicationInfo app_info {
      .pApplicationName = "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(0, 0, 1),
      .apiVersion = vk::ApiVersion14,
    };

    vk::InstanceCreateInfo create_info {
      .pApplicationInfo = &app_info,
      .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
    };

    return //
      vk_utils::check_vk_result(context_.createInstance(create_info),
        "Failed to create Vulkan instance, with result: {}")
        .transform([ this ](auto instance) noexcept -> auto
          { instance_ = std::move(instance); });
  }

  void
  print_extensions(std::span<const vk::ExtensionProperties> extensions)
  {
    for (const auto& extension : extensions)
    {
      std::println("\t{}", std::string_view { extension.extensionName });
    }
  }

  static VKAPI_ATTR auto VKAPI_CALL
  debug_callback(
    [[maybe_unused]] vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
    [[maybe_unused]] void* _) -> vk::Bool32
  {
    std::println(std::cerr, "validation layer: type {} msg: {}",
      vk::to_string(type), p_callback_data->pMessage);

    return vk::False;
  }

  auto
  is_device_suitable(const vk::raii::PhysicalDevice& device) -> bool
  {
    return has_minimum_api_version(device) &&
      has_graphics_queue_family(device) && has_required_extensions(device);
  }

  auto
  has_minimum_api_version(const vk::raii::PhysicalDevice& device) -> bool
  {
    return device.getProperties().apiVersion >= VK_API_VERSION_1_3;
  }

  auto
  has_graphics_queue_family(const vk::raii::PhysicalDevice& device) -> bool
  {
    auto queue_families = device.getQueueFamilyProperties();
    return std::ranges::any_of(queue_families,
      [](const vk::QueueFamilyProperties& qfp) noexcept -> bool
      {
        return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
          vk::QueueFlags { 0 };
      });
  }

  auto
  has_required_extensions(const vk::raii::PhysicalDevice& device) -> bool
  {
    auto extensions_result =
      vk_utils::check_vk_result(device.enumerateDeviceExtensionProperties(),
        "Failed to enumerate device extensions: {}");

    if (!extensions_result) { return false; }

    const auto& extensions = *extensions_result;
    return std::ranges::all_of(device_extensions,
      [ &extensions ](const char* required) noexcept -> bool
      {
        return std::ranges::any_of(extensions,
          [ required ](const auto& ext) noexcept -> bool
          { return std::strcmp(ext.extensionName, required) == 0; });
      });
  }

  auto
  get_queue_family_indices()
    -> std::pair<std::optional<std::uint32_t>, std::optional<std::uint32_t>>
  {
    std::optional<std::uint32_t> graphics_index;
    std::optional<std::uint32_t> presentation_index;

    auto queue_families = physical_device_.getQueueFamilyProperties();

    for (const auto& [ index, qfp ] : queue_families | std::views::enumerate)
    {
      const bool supports_graphics =
        (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags { 0 };

      const auto [ result, supports_presentation ] =
        physical_device_.getSurfaceSupportKHR(
          static_cast<std::uint32_t>(index), *surface_);

      if (supports_graphics && !graphics_index) { graphics_index = index; }

      if ((supports_presentation != vk::Bool32 { 0 }) && !presentation_index)
      {
        presentation_index = index;
      }

      if (supports_graphics && (supports_presentation != vk::Bool32 { 0 }))
      {
        graphics_index = presentation_index = index;
        break;
      }
    }

    return std::make_pair(graphics_index, presentation_index);
  }

private:
  GLFWwindow* window_ {};
  vk::raii::Context context_;
  vk::raii::Instance instance_ { nullptr };
  vk::raii::DebugUtilsMessengerEXT debug_messenger_ { nullptr };
  vk::raii::SurfaceKHR surface_ { nullptr };
  vk::raii::PhysicalDevice physical_device_ { nullptr };
  vk::raii::Device device_ { nullptr };
  vk::raii::Queue graphics_queue_ { nullptr };
  vk::raii::Queue presentation_queue_ { nullptr };
};