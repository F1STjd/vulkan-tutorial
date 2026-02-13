#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <print>
#include <ranges>
#include <utility>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_USE_STD_EXPECTED
#include "vulkan/vulkan.hpp"
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../lib/load/load.hpp"
#include "apputils.hpp"
#include "vkutils.hpp"

static constexpr std::uint32_t initial_width { 800 };
static constexpr std::uint32_t initial_height { 600 };

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
  constexpr void
  run()
  {
    auto init_result = //
      init_window().and_then([ this ] -> std::expected<void, vkutils::error>
        { return init_vulkan(); });

    if (!init_result.has_value()) [[unlikely]]
    {
      const auto& error = init_result.error();
      const auto& file = error.location.file_name();
      const auto& function = error.location.function_name();
      const auto& line = error.location.line();
      const auto& column = error.location.column();

      std::println(std::cerr,
        "{}: In function '{}':\n{}:{}:{}: error: during initialisation of an "
        "Vulkan object got result code: {}",
        file, function, file, line, column, error.to_string());
      std::exit(EXIT_FAILURE);
    }
    main_loop();
    cleanup();
  }

private:
  // TODO(Konrad): change error type to some glfw related one
  constexpr auto
  init_window() -> std::expected<void, vkutils::error>
  {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ = glfwCreateWindow(
      initial_width, initial_height, "Vulkan", nullptr, nullptr);

    return {};
  }

  constexpr auto
  init_vulkan() -> std::expected<void, vkutils::error>
  {
    return create_instance()
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return setup_debug_messenger(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_surface(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return pick_physical_device(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_logical_device(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_swap_chain(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_image_views(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_graphics_pipeline(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_command_pool(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_command_buffer(); });
  }

  constexpr void
  main_loop()
  {
    while (glfwWindowShouldClose(window_) == 0)
    {
      glfwPollEvents();
    }
  }

  constexpr void
  cleanup()
  {
    glfwDestroyWindow(window_);
    glfwTerminate();
  }

  constexpr auto
  create_instance() -> std::expected<void, vkutils::error>
  {
    std::vector<const char*> layers;
    std::vector<const char*> extensions;

    return get_required_layers(layers)
      .and_then([ &, this ]() -> std::expected<void, vkutils::error>
        { return get_required_extensions(extensions); })
      .and_then([ &, this ]() -> std::expected<void, vkutils::error>
        { return create_vulkan_instance(layers, extensions); });
  }

  constexpr auto
  setup_debug_messenger() -> std::expected<void, vkutils::error>
  {
    if (!enable_validation_layers) { return {}; }

    constexpr vk::DebugUtilsMessageSeverityFlagsEXT severity_flags {
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    };
    constexpr vk::DebugUtilsMessageTypeFlagsEXT message_type_flags {
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
    };
    constexpr vk::DebugUtilsMessengerCreateInfoEXT
      debug_utils_messenger_info_EXT {
        .messageSeverity = severity_flags,
        .messageType = message_type_flags,
        .pfnUserCallback = &debug_callback,
      };

    return vkutils::locate(
      instance_.createDebugUtilsMessengerEXT(debug_utils_messenger_info_EXT))
      .transform(vkutils::store_into(debug_messenger_));
  }

  constexpr auto
  create_surface() -> std::expected<void, vkutils::error>
  {
    VkSurfaceKHR _surface {};
    if (glfwCreateWindowSurface(*instance_, window_, nullptr, &_surface) != 0)
      [[unlikely]]
    {
      return std::expected<void, vkutils::error> {
        std::unexpect,
        apputils::error::glfw_surface_creation_failed,
      };
    }

    surface_ = vk::raii::SurfaceKHR(instance_, _surface);
    return {};
  }

  constexpr auto
  pick_physical_device() -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(instance_.enumeratePhysicalDevices())
      .and_then(
        [ this ](const auto& devices) -> std::expected<void, vkutils::error>
        {
          for (const auto& device : devices)
          {
            if (is_device_suitable(device))
            {
              physical_device_ = device;
              return {};
            }
          }
          return std::expected<void, vkutils::error> {
            std::unexpect,
            apputils::error::no_suitable_gpu,
          };
        });
  }

  constexpr auto
  create_logical_device() -> std::expected<void, vkutils::error>
  {
    const auto [ graphics_index, presentation_index ] =
      get_queue_family_indices();

    if (!graphics_index || !presentation_index)
    {
      return std::expected<void, vkutils::error> {
        std::unexpect,
        apputils::error::missing_queue_families,
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
    vk::DeviceQueueCreateInfo device_queue_info {
      .queueFamilyIndex = *graphics_index,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
    };

    vk::DeviceCreateInfo device_info {
      .pNext = &feature_chain.get(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &device_queue_info,
      .enabledExtensionCount =
        static_cast<std::uint32_t>(device_extensions.size()),
      .ppEnabledExtensionNames = device_extensions.data(),
    };

    return vkutils::locate(physical_device_.createDevice(device_info))
      .transform(
        [ this, graphics_index ](auto&& device) noexcept -> auto
        {
          device_ = std::forward<decltype(device)>(device);
          graphics_queue_ = device_.getQueue(*graphics_index, 0);
          // presentation_queue_ = device_.getQueue(*presentation_index, 0);
        });
  }

  constexpr auto
  create_swap_chain() -> std::expected<void, vkutils::error>
  {
    vk::SurfaceCapabilitiesKHR surface_capabilities;
    std::vector<vk::SurfaceFormatKHR> surface_formats;
    std::vector<vk::PresentModeKHR> surface_presentation_modes;

    return get_surface_capabilities(surface_capabilities)
      .and_then([ &, this ]() noexcept -> auto
        { return get_surface_formats(surface_formats); })
      .and_then([ &, this ]() noexcept -> auto
        { return get_surface_present_modes(surface_presentation_modes); })
      .transform(
        [ &, this ]() noexcept -> void
        {
          swap_chain_extent_ = choose_swap_extent(surface_capabilities);
          swap_chain_surface_format_ =
            choose_swap_surface_format(surface_formats);
        })
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return create_swap_chain(
            surface_capabilities, surface_presentation_modes);
        });
  }

  constexpr auto
  create_image_views() -> std::expected<void, vkutils::error>
  {
    swap_chain_image_views_.clear();

    vk::ImageViewCreateInfo image_view_info {
      .pNext = {},
      .flags = {},
      .image = {},
      .viewType = vk::ImageViewType::e2D,
      .format = swap_chain_surface_format_.format,
      .components = {},
      .subresourceRange {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    for (const auto& image : swap_chain_images_)
    {
      image_view_info.image = image;
      auto image_view =
        vkutils::locate(device_.createImageView(image_view_info));
      if (!image_view.has_value()) [[unlikely]]
      {
        return std::unexpected { image_view.error() };
      }
      swap_chain_image_views_.emplace_back(std::move(image_view.value()));
    }

    return {};
  }

  // TODO(Konrad): refactor - too long
  constexpr auto
  create_graphics_pipeline() -> std::expected<void, vkutils::error>
  {
    auto shader_module_result =
      vkutils::locate(load::shader({ SHADER_DIR "/slang.spv" }))
        .and_then([ this ](std::span<const char> code) -> auto
          { return create_shader_module(code); });

    if (!shader_module_result.has_value()) [[unlikely]]
    {
      return std::unexpected { shader_module_result.error() };
    }

    const auto& shader_module = *shader_module_result;

    std::array shader_stages_info {
      vk::PipelineShaderStageCreateInfo {
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shader_module,
        .pName = "vertex_main",
        .pSpecializationInfo = nullptr,
      },
      vk::PipelineShaderStageCreateInfo {
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shader_module,
        .pName = "fragment_main",
        .pSpecializationInfo = nullptr,
      },
    };

    vk::PipelineVertexInputStateCreateInfo vertex_input_info;
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_info {
      .topology = vk::PrimitiveTopology::eTriangleList
    };

    vk::PipelineViewportStateCreateInfo viewport_state_info {
      .flags = {},
      .viewportCount = 1,
      .pViewports = {},
      .scissorCount = 1,
      .pScissors = {},
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer_info {
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eClockwise,
      .depthBiasEnable = vk::False,
      .depthBiasSlopeFactor = 1.0F,
      .lineWidth = 1.0F,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling_info {
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False,
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment_info {
      .blendEnable = vk::False,
      .colorWriteMask = vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo color_blending_info {
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment_info,
    };

    std::array dynamic_states {
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state_info {
      .flags = {},
      .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data(),
    };

    vk::PipelineLayoutCreateInfo pipeline_layout_info {
      .setLayoutCount = 0,
      .pushConstantRangeCount = 0,
    };

    return vkutils::locate(device_.createPipelineLayout(pipeline_layout_info))
      .transform(vkutils::store_into(pipeline_layout_))
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          vk::StructureChain pipeline_info_chain {
            vk::GraphicsPipelineCreateInfo {
              .stageCount =
                static_cast<std::uint32_t>(shader_stages_info.size()),
              .pStages = shader_stages_info.data(),
              .pVertexInputState = &vertex_input_info,
              .pInputAssemblyState = &input_assembly_info,
              .pViewportState = &viewport_state_info,
              .pRasterizationState = &rasterizer_info,
              .pMultisampleState = &multisampling_info,
              .pColorBlendState = &color_blending_info,
              .pDynamicState = &dynamic_state_info,
              .layout = *pipeline_layout_,
              .renderPass = nullptr,
            },
            vk::PipelineRenderingCreateInfo {
              .colorAttachmentCount = 1,
              .pColorAttachmentFormats = &swap_chain_surface_format_.format,
            },
          };

          return vkutils::locate(
            device_.createGraphicsPipeline(nullptr, pipeline_info_chain.get()))
            .transform(vkutils::store_into(graphics_pipeline_));
        });
  }

  constexpr auto
  create_command_pool() -> std::expected<void, vkutils::error>
  {
    vk::CommandPoolCreateInfo command_pool_info {
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = graphics_queue_index_,
    };
    return vkutils::locate(device_.createCommandPool(command_pool_info))
      .transform(vkutils::store_into(command_pool_));
  }

  constexpr auto
  create_command_buffer() -> std::expected<void, vkutils::error>
  {
    vk::CommandBufferAllocateInfo command_buffer_allocate_info {
      .commandPool = command_pool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
    };

    return vkutils::locate(
      device_.allocateCommandBuffers(command_buffer_allocate_info))
      .transform([ this ](auto&& command_buffers) -> auto
        { command_buffer_ = std::move(command_buffers.front()); });
  }

  constexpr auto
  record_command_buffer(std::uint32_t image_index)
    -> std::expected<void, vkutils::error>
  {
    if (const auto result = command_buffer_.begin({}); !result.has_value())
    {
      return vkutils::locate(result);
    }

    transition_image_layout(image_index, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits2 {},
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::ClearValue clear_color = vk::ClearColorValue { 0.0F, 0.0F, 0.0F, 1.0F };
    vk::RenderingAttachmentInfo attachment_info {
      .imageView = swap_chain_image_views_[ image_index ],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clear_color,
    };
    vk::RenderingInfo rendering_info {
      .renderArea {
        .offset { .x = 0, .y = 0 },
        .extent = swap_chain_extent_,
      },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachment_info,
    };

    command_buffer_.beginRendering(rendering_info);
    command_buffer_.bindPipeline(
      vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    command_buffer_.setViewport(0,
      vk::Viewport {
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(swap_chain_extent_.width),
        .height = static_cast<float>(swap_chain_extent_.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
      });
    command_buffer_.setScissor(0,
      vk::Rect2D {
        .offset { .x = 0, .y = 0 },
        .extent = swap_chain_extent_,
      });
    command_buffer_.draw(3, 1, 0, 0);
    command_buffer_.endRendering();

    transition_image_layout(image_index,
      vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2 {},
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eBottomOfPipe);

    return vkutils::locate(command_buffer_.end());
  }

private:
  // TODO(Konrad): Refactor - maybe the vkutils::validate_required is bad
  // design
  constexpr auto
  get_required_layers(std::vector<const char*>& required_layers)
    -> std::expected<void, vkutils::error>
  {
    if constexpr (enable_validation_layers)
    {
      required_layers.assign_range(validation_layers);
    }

    return vkutils::locate(context_.enumerateInstanceLayerProperties())
      .and_then(
        [ &required_layers ](const auto& layer_properties) -> auto
        {
          return vkutils::locate(
            vkutils::validate_required(std::move(required_layers),
              layer_properties, [](const auto& prop) noexcept -> auto
              { return prop.layerName.data(); }));
        })
      .transform(vkutils::store_into(required_layers));
  }

  // TODO(Konrad): Refactor - maybe the vkutils::validate_required is bad
  // design
  constexpr auto
  get_required_extensions(std::vector<const char*>& required_extensions)
    -> std::expected<void, vkutils::error>
  {
    std::uint32_t glfw_extension_count {};
    const auto* glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_extension_count);

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

    return vkutils::locate(context_.enumerateInstanceExtensionProperties())
      .and_then(
        [ &required_extensions ](const auto& extension_properties) -> auto
        {
          return vkutils::locate(
            vkutils::validate_required(std::move(required_extensions),
              extension_properties, [](const auto& prop) noexcept -> auto
              { return prop.extensionName.data(); }));
        })
      .transform(vkutils::store_into(required_extensions));
  }

  constexpr auto
  create_vulkan_instance(std::span<const char* const> layers,
    std::span<const char* const> extensions)
    -> std::expected<void, vkutils::error>
  {
    constexpr vk::ApplicationInfo app_info {
      .pApplicationName = "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(0, 0, 1),
      .apiVersion = vk::ApiVersion14,
    };

    const vk::InstanceCreateInfo info {
      .pApplicationInfo = &app_info,
      .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
    };

    return vkutils::locate(context_.createInstance(info))
      .transform(vkutils::store_into(instance_));
  }

  constexpr void
  print_extensions(std::span<const vk::ExtensionProperties> extensions)
  {
    for (const auto& extension : extensions)
    {
      std::println("\t{}", std::string_view { extension.extensionName });
    }
  }

  static constexpr VKAPI_ATTR auto VKAPI_CALL
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

  constexpr auto
  is_device_suitable(const vk::raii::PhysicalDevice& device) -> bool
  {
    return has_minimum_api_version(device) &&
      has_graphics_queue_family(device) && has_required_extensions(device);
  }

  constexpr auto
  has_minimum_api_version(const vk::raii::PhysicalDevice& device) -> bool
  {
    return device.getProperties().apiVersion >= VK_API_VERSION_1_3;
  }

  constexpr auto
  has_graphics_queue_family(const vk::raii::PhysicalDevice& device) -> bool
  {
    const auto queue_families = device.getQueueFamilyProperties();
    return std::ranges::any_of(queue_families,
      [](const vk::QueueFamilyProperties& qfp) noexcept -> bool
      {
        return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
          vk::QueueFlags { 0 };
      });
  }

  constexpr auto
  has_required_extensions(const vk::raii::PhysicalDevice& device) -> bool
  {
    const auto extensions_result =
      vkutils::locate(device.enumerateDeviceExtensionProperties());

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

  // TODO(Konrad): fix this monstrosity
  constexpr auto
  get_queue_family_indices()
    -> std::pair<std::optional<std::uint32_t>, std::optional<std::uint32_t>>
  {
    std::optional<std::uint32_t> graphics_index;
    std::optional<std::uint32_t> presentation_index;

    const auto queue_families = physical_device_.getQueueFamilyProperties();

    for (const auto& [ index, qfp ] : queue_families | std::views::enumerate)
    {
      const bool supports_graphics =
        (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags { 0 };

      const auto surface_support_result = physical_device_.getSurfaceSupportKHR(
        static_cast<std::uint32_t>(index), *surface_);

      if (!surface_support_result.has_value())
      {
        return { std::nullopt, std::nullopt };
      }

      if (supports_graphics && !graphics_index) { graphics_index = index; }

      if ((*surface_support_result != vk::Bool32 { 0 }) && !presentation_index)
      {
        presentation_index = index;
      }

      if (supports_graphics && (*surface_support_result != vk::Bool32 { 0 }))
      {
        graphics_index = presentation_index = index;
        break;
      }
    }

    return std::make_pair(graphics_index, presentation_index);
  }

  constexpr auto
  choose_swap_surface_format(
    std::span<const vk::SurfaceFormatKHR> available_formats)
    -> vk::SurfaceFormatKHR
  {
    if (const auto format = std::ranges::find_if(available_formats,
          [](const vk::SurfaceFormatKHR& available_format) noexcept -> bool
          {
            return available_format.format == vk::Format::eB8G8R8A8Srgb &&
              available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
          });
      format != available_formats.end())
    {
      return *format;
    }
    return available_formats[ 0 ];
  }

  constexpr auto
  choose_swap_presentation_mode(
    std::span<const vk::PresentModeKHR> available_presentation_modes)
    -> vk::PresentModeKHR
  {
    if (const auto mode = std::ranges::find(
          available_presentation_modes, vk::PresentModeKHR::eMailbox);
      mode != available_presentation_modes.end())
    {
      return *mode;
    }
    return vk::PresentModeKHR::eFifo;
  }

  constexpr auto
  choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities)
    -> vk::Extent2D
  {
    if (capabilities.currentExtent.width !=
      std::numeric_limits<std::uint32_t>::max())
    {
      return capabilities.currentExtent;
    }

    std::int32_t width {};
    std::int32_t height {};
    glfwGetFramebufferSize(window_, &width, &height);

    return vk::Extent2D {
      .width = std::clamp<std::uint32_t>(static_cast<std::uint32_t>(width),
        capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
      .height = std::clamp<std::uint32_t>(static_cast<std::uint32_t>(height),
        capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
  }

  static constexpr auto
  choose_swap_min_image_count(
    const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> std::uint32_t
  {
    auto min_image_count = std::max(3U, surface_capabilities.minImageCount);
    if ((0 < surface_capabilities.maxImageCount) &&
      (surface_capabilities.maxImageCount < min_image_count))
    {
      min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
  }

  constexpr auto
  get_surface_capabilities(vk::SurfaceCapabilitiesKHR& surface_capabilities)
    -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(
      physical_device_.getSurfaceCapabilitiesKHR(*surface_))
      .transform(vkutils::store_into(surface_capabilities));
  }

  constexpr auto
  get_surface_formats(std::vector<vk::SurfaceFormatKHR>& surface_formats)
    -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(physical_device_.getSurfaceFormatsKHR(*surface_))
      .transform(vkutils::store_into(surface_formats));
  }

  constexpr auto
  get_surface_present_modes(
    std::vector<vk::PresentModeKHR>& surface_presentation_modes)
    -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(
      physical_device_.getSurfacePresentModesKHR(*surface_))
      .transform(vkutils::store_into(surface_presentation_modes));
  }

  constexpr auto
  create_swap_chain(const vk::SurfaceCapabilitiesKHR& surface_capabilities,
    std::span<const vk::PresentModeKHR> surface_presentation_modes)
    -> std::expected<void, vkutils::error>
  {
    const vk::SwapchainCreateInfoKHR swap_chain_info {
      .flags = vk::SwapchainCreateFlagsKHR(),
      .surface = *surface_,
      .minImageCount = choose_swap_min_image_count(surface_capabilities),
      .imageFormat = swap_chain_surface_format_.format,
      .imageColorSpace = swap_chain_surface_format_.colorSpace,
      .imageExtent = swap_chain_extent_,
      .imageArrayLayers = 1U,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surface_capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = choose_swap_presentation_mode(surface_presentation_modes),
      .clipped = vk::True,
      .oldSwapchain = nullptr,
    };

    return create_swap_chain_impl(swap_chain_info)
      .and_then([ this ]() -> std::expected<void, vk_utils::error>
        { return get_swap_chain_images(); });
  }

  constexpr auto
  create_swap_chain_impl(const vk::SwapchainCreateInfoKHR& swap_chain_info)
    -> std::expected<void, vk_utils::error>
  {
    return vk_utils::locate(device_.createSwapchainKHR(swap_chain_info))
      .transform(vk_utils::store_into(swap_chain_));
  }

  constexpr auto
  get_swap_chain_images() -> std::expected<void, vk_utils::error>
  {
    return vk_utils::locate(swap_chain_.getImages())
      .transform(vk_utils::store_into(swap_chain_images_));
  }

  [[nodiscard]]
  constexpr auto
  create_shader_module(std::span<const char> code)
    -> std::expected<vk::raii::ShaderModule, vk_utils::error>
  {
    vk::ShaderModuleCreateInfo info {
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const std::uint32_t*>(code.data()),
    };

    return vk_utils::locate(device_.createShaderModule(info));
  }

private:
  // 8 bytes alignment
  GLFWwindow* window_ {};
  vk::raii::Context context_;
  vk::raii::Instance instance_ { nullptr };
  vk::raii::DebugUtilsMessengerEXT debug_messenger_ { nullptr };
  vk::raii::SurfaceKHR surface_ { nullptr };
  vk::raii::PhysicalDevice physical_device_ { nullptr };
  vk::raii::Device device_ { nullptr };
  vk::raii::Queue graphics_queue_ { nullptr };
  vk::raii::Queue presentation_queue_ { nullptr };
  vk::raii::SwapchainKHR swap_chain_ { nullptr };
  std::vector<vk::Image> swap_chain_images_;
  std::vector<vk::raii::ImageView> swap_chain_image_views_;
  vk::raii::PipelineLayout pipeline_layout_ { nullptr };
  vk::raii::Pipeline graphics_pipeline_ { nullptr };
  vk::raii::CommandPool command_pool_ { nullptr };

  // 4 bytes alignment
  vk::SurfaceFormatKHR swap_chain_surface_format_ {};
  vk::Extent2D swap_chain_extent_ {};
  std::uint32_t graphics_queue_index_ { ~0U };
};