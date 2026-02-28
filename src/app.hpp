#pragma once

#include "../lib/load/load.hpp"
#include "apputils.hpp"
#include "uniforms.hpp"
#include "vertex.hpp"
#include "vkutils.hpp"

static constexpr std::uint32_t initial_width { 800 };
static constexpr std::uint32_t initial_height { 600 };
static const std::string model_path { MODELS_DIR "/viking_room.obj" };
static const std::string texture_path {
  TEXTURES_DIR "/viking_room.png",
};

static constexpr std::int32_t max_frames_in_flight { 2 };

static constexpr std::array validation_layers { "VK_LAYER_KHRONOS_validation" };
static constexpr std::array device_extensions {
  vk::KHRSwapchainExtensionName,
  vk::KHRShaderDrawParametersExtensionName,
  vk::KHRSynchronization2ExtensionName,
};

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
    const auto result =
      init_window()
        .and_then([ this ] -> std::expected<void, vkutils::error>
          { return init_vulkan(); })
        .and_then([ this ] -> std::expected<void, vkutils::error>
          { return main_loop(); });

    if (!result.has_value()) [[unlikely]] { report_error(result.error()); }
    cleanup();
  }

  static constexpr void
  report_error(const vkutils::error& error)
  {
    const auto& file = error.location.file_name();
    const auto& function = error.location.function_name();
    const auto& line = error.location.line();
    const auto& column = error.location.column();

    std::println(std::cerr, "{}: In function '{}':\n{}:{}:{}: error: {}", file,
      function, file, line, column, error.to_string());
  }

private:
  constexpr auto
  init_window() -> std::expected<void, vkutils::error>
  {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(
      initial_width, initial_height, "Vulkan", nullptr, nullptr);

    if (window_ == nullptr)
    {
      return std::unexpected {
        vkutils::error {
          .reason = apputils::error::glfw_window_creation_failed,
        },
      };
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebuffer_resize_callback);

    return {};
  }

  static constexpr void
  framebuffer_resize_callback(GLFWwindow* window,
    [[maybe_unused]] std::int32_t width, [[maybe_unused]] std::int32_t height)
  {
    auto* const window_ptr =
      reinterpret_cast<app*>(glfwGetWindowUserPointer(window));
    window_ptr->framebuffer_resized_ = true;
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
        { return create_swapchain(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_image_views(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_descriptor_set_layout(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_command_pool(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_color_resources(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_depth_resources(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_graphics_pipeline(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_texture_image(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_texture_image_view(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_texture_sampler(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return load_model(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_vertex_buffer(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_index_buffer(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_uniform_buffers(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_descriptor_pool(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_descriptor_sets(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_command_buffers(); })
      .and_then([ this ] -> std::expected<void, vkutils::error>
        { return create_sync_objects(); });
  }

  constexpr auto
  main_loop() -> std::expected<void, vkutils::error>
  {
    while (glfwWindowShouldClose(window_) == 0)
    {
      glfwPollEvents();
      if (auto result = draw_frame(); !result) [[unlikely]] { return result; }
    }

    return vkutils::locate(device_.waitIdle());
  }

  constexpr void
  cleanup()
  {
    cleanup_swapchain();

    glfwDestroyWindow(window_);
    glfwTerminate();
  }

  // TODO(Konrad): Layers and extensions are checked if supported, not "got" -
  // maybe change the name of the functions
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
      return std::unexpected {
        vkutils::error {
          .reason = apputils::error::glfw_surface_creation_failed,
        },
      };
    }

    surface_ = vk::raii::SurfaceKHR { instance_, _surface };
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
              msaa_samples_ = get_max_usable_sample_count();
              return {};
            }
          }
          return std::unexpected {
            vkutils::error {
              .reason = apputils::error::no_suitable_gpu,
            },
          };
        });
  }

  // TODO(Konrad): tutorial was mixing 2-way approach (graphics/presentation
  // queue) with single queue. Read about the advantages/requirements that would
  // be more suitable (where and when)
  constexpr auto
  create_logical_device() -> std::expected<void, vkutils::error>
  {
    const auto [ graphics_index, presentation_index ] =
      get_queue_family_indices();

    if (!graphics_index || !presentation_index)
    {
      return std::unexpected {
        vkutils::error {
          .reason = apputils::error::missing_queue_families,
        },
      };
    }
    graphics_queue_index_ = *graphics_index;
    presentation_queue_index_ = *presentation_index;

    vk::StructureChain feature_chain {
      vk::PhysicalDeviceFeatures2 {
        .features {
          .samplerAnisotropy = vk::True,
        },
      },
      vk::PhysicalDeviceVulkan13Features {
        .synchronization2 = vk::True,
        .dynamicRendering = vk::True,
      },
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT {
        .extendedDynamicState = vk::True,
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
  create_swapchain() -> std::expected<void, vkutils::error>
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
          swapchain_extent_ = choose_swap_extent(surface_capabilities);
          swapchain_surface_format_ =
            choose_swap_surface_format(surface_formats);
        })
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return create_swapchain(
            surface_capabilities, surface_presentation_modes);
        });
  }

  constexpr auto
  create_image_views() -> std::expected<void, vkutils::error>
  {
    swapchain_image_views_.clear();

    for (auto& image : swapchain_images_)
    {
      auto image_view = create_image_view(image,
        swapchain_surface_format_.format, vk::ImageAspectFlagBits::eColor, 1);
      if (!image_view) [[unlikely]]
      {
        return std::unexpected { image_view.error() };
      }
      swapchain_image_views_.emplace_back(std::move(*image_view));
    }

    return {};
  }

  constexpr auto
  create_texture_sampler() -> std::expected<void, vkutils::error>
  {
    auto properties = physical_device_.getProperties();
    vk::SamplerCreateInfo sampler_info {
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias = 0.0F,
      .anisotropyEnable = vk::True,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways,
      .minLod = 0.0F,
      .maxLod = vk::LodClampNone,
      .borderColor = vk::BorderColor::eIntOpaqueBlack,
      .unnormalizedCoordinates = vk::False,
    };

    return vkutils::locate(device_.createSampler(sampler_info))
      .transform(vkutils::store_into(texture_sampler_));
  }

  constexpr auto
  create_descriptor_set_layout() -> std::expected<void, vkutils::error>
  {
    constexpr std::array bindings {
      vk::DescriptorSetLayoutBinding {
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr,
      },
      vk::DescriptorSetLayoutBinding {
        .binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
      },
    };

    vk::DescriptorSetLayoutCreateInfo layout_info {
      .bindingCount = bindings.size(),
      .pBindings = bindings.data(),
    };

    return vkutils::locate(device_.createDescriptorSetLayout(layout_info))
      .transform(vkutils::store_into(descriptor_set_layout_));
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

    constexpr auto binding_description = vertex::get_binding_description();
    constexpr auto attribute_descriptions =
      vertex::get_attribute_descriptions();
    vk::PipelineVertexInputStateCreateInfo vertex_input_info {
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_description,
      .vertexAttributeDescriptionCount =
        static_cast<std::uint32_t>(attribute_descriptions.size()),
      .pVertexAttributeDescriptions = attribute_descriptions.data(),
    };

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
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::False,
      .depthBiasSlopeFactor = 1.0F,
      .lineWidth = 1.0F,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling_info {
      .rasterizationSamples = msaa_samples_,
      .sampleShadingEnable = vk::False,
    };

    vk::PipelineDepthStencilStateCreateInfo depth_stencil_info {
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLess,
      .depthBoundsTestEnable = vk::False,
      .stencilTestEnable = vk::False,
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
      .setLayoutCount = 1,
      .pSetLayouts = &*descriptor_set_layout_,
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
              .pDepthStencilState = &depth_stencil_info,
              .pColorBlendState = &color_blending_info,
              .pDynamicState = &dynamic_state_info,
              .layout = *pipeline_layout_,
              .renderPass = nullptr,
            },
            vk::PipelineRenderingCreateInfo {
              .colorAttachmentCount = 1,
              .pColorAttachmentFormats = &swapchain_surface_format_.format,
              .depthAttachmentFormat = depth_format_,
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
  create_color_resources() -> std::expected<void, vkutils::error>
  {
    vk::Format color_format = swapchain_surface_format_.format;

    return create_image(swapchain_extent_.width, swapchain_extent_.height, 1,
      msaa_samples_, color_format, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransientAttachment |
        vk::ImageUsageFlagBits::eColorAttachment,
      vk::MemoryPropertyFlagBits::eDeviceLocal, color_image_,
      color_image_memory_)
      .and_then(
        [ &, this ]() noexcept
          -> std::expected<vk::raii::ImageView, vkutils::error>
        {
          return create_image_view(
            color_image_, color_format, vk::ImageAspectFlagBits::eColor, 1);
        })
      .transform(vkutils::store_into(color_image_view_));
  }

  constexpr auto
  create_depth_resources() -> std::expected<void, vkutils::error>
  {
    return find_depth_format()
      .transform(vkutils::store_into(depth_format_))
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return create_image(swapchain_extent_.width, swapchain_extent_.height,
            1U, msaa_samples_, depth_format_, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal, depth_image_,
            depth_image_memory_);
        })
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return create_image_view(
            depth_image_, depth_format_, vk::ImageAspectFlagBits::eDepth, 1)
            .transform(vkutils::store_into(depth_image_view_));
        });
  }

  // maybe some struct of an image would be better
  constexpr auto
  create_texture_image() -> std::expected<void, vkutils::error>
  {
    std::int32_t texture_width {};
    std::int32_t texture_height {};
    std::int32_t texture_channels {};

    auto* pixels = stbi_load(texture_path.c_str(), &texture_width,
      &texture_height, &texture_channels, STBI_rgb_alpha);
    const vk::DeviceSize image_size { static_cast<std::size_t>(texture_width) *
      static_cast<std::size_t>(texture_height) * 4UZ };
    mip_levels_ = static_cast<std::uint32_t>(std::floor(
                    std::log2(std::max(texture_width, texture_height)))) +
      1;

    const auto u_texture_width = static_cast<std::uint32_t>(texture_width);
    const auto u_texture_height = static_cast<std::uint32_t>(texture_height);

    if (pixels == nullptr)
    {
      return std::unexpected {
        vkutils::error { .reason = apputils::error::stb_load_failed },
      };
    }

    vk::raii::Buffer staging_buffer { nullptr };
    vk::raii::DeviceMemory staging_buffer_memory { nullptr };
    return create_buffer(image_size, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
      staging_buffer, staging_buffer_memory)
      .and_then(
        [ & ]() noexcept -> std::expected<void, vkutils::error>
        {
          return map_memory(image_size, staging_buffer_memory,
            std::span { pixels, image_size });
        })
      .transform([ & ]() -> void { stbi_image_free(pixels); })
      .and_then(
        [ &, this ]() -> std::expected<void, vkutils::error>
        {
          return create_image(u_texture_width, u_texture_height, mip_levels_,
            vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferSrc |
              vk::ImageUsageFlagBits::eTransferDst |
              vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal, texture_image_,
            texture_image_memory_);
        })
      .and_then(
        [ this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return transition_image_layout(texture_image_,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            mip_levels_);
        })
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return copy_buffer_to_image(
            staging_buffer, texture_image_, u_texture_width, u_texture_height);
        })
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return generate_mip_maps(texture_image_, vk::Format::eR8G8B8A8Srgb,
            texture_width, texture_height, mip_levels_);
        });
  }

  constexpr auto
  create_texture_image_view() -> std::expected<void, vkutils::error>
  {
    return create_image_view(texture_image_, vk::Format::eR8G8B8A8Srgb,
      vk::ImageAspectFlagBits::eColor, mip_levels_)
      .transform(vkutils::store_into(texture_image_view_));
  }

  constexpr auto
  load_model() -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(
      load::model_obj(vertices_, indices_, model_path.c_str()));
  }

  // https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/03_Index_buffer.html
  // says: The previous chapter already mentioned that you should allocate
  // multiple resources like buffers from a single memory allocation, but in
  // fact you should go a step further. Driver developers recommend that you
  // also store multiple buffers, like the vertex and index buffer, into a
  // single VkBuffer and use offsets in commands like vkCmdBindVertexBuffers.
  // The advantage is that your data is more cache friendly in that case,
  // because it’s closer together. It is even possible to reuse the same chunk
  // of memory for multiple resources if they are not used during the same
  // render operations, provided that their data is refreshed, of course. This
  // is known as aliasing and some Vulkan functions have explicit flags to
  // specify that you want to do this.
  constexpr auto
  create_vertex_buffer() -> std::expected<void, vkutils::error>
  {
    const vk::DeviceSize buffer_size = sizeof(vertex) * vertices_.size();

    vk::raii::Buffer staging_buffer { nullptr };
    vk::raii::DeviceMemory staging_buffer_memory { nullptr };

    return create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
      staging_buffer, staging_buffer_memory)
      .and_then([ & ]() noexcept -> std::expected<void, vkutils::error>
        { return map_memory(buffer_size, staging_buffer_memory, vertices_); })
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return create_buffer(buffer_size,
            vk::BufferUsageFlagBits::eVertexBuffer |
              vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, vertex_buffer_,
            vertex_buffer_memory_);
        })
      .and_then([ &, this ]() noexcept -> std::expected<void, vkutils::error>
        { return copy_buffer(staging_buffer, vertex_buffer_, buffer_size); });
  }

  constexpr auto
  create_index_buffer() -> std::expected<void, vkutils::error>
  {
    const vk::DeviceSize buffer_size = sizeof(indices_[ 0 ]) * indices_.size();

    vk::raii::Buffer staging_buffer { nullptr };
    vk::raii::DeviceMemory staging_buffer_memory { nullptr };

    return create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
      staging_buffer, staging_buffer_memory)
      .and_then([ & ]() noexcept -> std::expected<void, vkutils::error>
        { return map_memory(buffer_size, staging_buffer_memory, indices_); })
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return create_buffer(buffer_size,
            vk::BufferUsageFlagBits::eIndexBuffer |
              vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, index_buffer_,
            index_buffer_memory_);
        })
      .and_then([ &, this ]() noexcept -> std::expected<void, vkutils::error>
        { return copy_buffer(staging_buffer, index_buffer_, buffer_size); });
  }

  constexpr auto
  create_uniform_buffers() -> std::expected<void, vkutils::error>
  {
    uniform_buffers_.clear();
    uniform_buffers_memory_.clear();
    uniform_buffers_mapped_.clear();

    for (auto i : std::views::iota(0, max_frames_in_flight))
    {
      const auto si = static_cast<std::size_t>(i);
      vk::DeviceSize buffer_size = sizeof(uniform_buffer_object);
      vk::raii::Buffer buffer { nullptr };
      vk::raii::DeviceMemory buffer_memory { nullptr };

      auto result =
        create_buffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
          buffer, buffer_memory)
          .and_then(
            [ &, this ]() noexcept -> std::expected<void, vkutils::error>
            {
              uniform_buffers_.emplace_back(std::move(buffer));
              uniform_buffers_memory_.emplace_back(std::move(buffer_memory));
              const auto mapped =
                uniform_buffers_memory_[ si ].mapMemory(0, buffer_size);
              if (!mapped) [[unlikely]]
              {
                return std::unexpected {
                  vkutils::error {
                    .reason = mapped.error(),
                  },
                };
              }
              uniform_buffers_mapped_.emplace_back(*mapped);
              return {};
            });

      if (!result) [[unlikely]] { return result; }
    }

    return {};
  }

  constexpr auto
  create_descriptor_pool() -> std::expected<void, vkutils::error>
  {
    std::array pool_sizes {
      vk::DescriptorPoolSize {
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = max_frames_in_flight,
      },
      vk::DescriptorPoolSize {
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = max_frames_in_flight,
      },
    };

    vk::DescriptorPoolCreateInfo pool_info {
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = max_frames_in_flight,
      .poolSizeCount = pool_sizes.size(),
      .pPoolSizes = pool_sizes.data(),
    };

    return vkutils::locate(device_.createDescriptorPool(pool_info))
      .transform(vkutils::store_into(descriptor_pool_));
  }

  constexpr auto
  create_descriptor_sets() -> std::expected<void, vkutils::error>
  {
    std::vector<vk::DescriptorSetLayout> layouts(
      max_frames_in_flight, *descriptor_set_layout_);
    vk::DescriptorSetAllocateInfo allocate_info {
      .descriptorPool = descriptor_pool_,
      .descriptorSetCount = static_cast<std::uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data(),
    };

    descriptor_sets_.clear();
    return vkutils::locate(device_.allocateDescriptorSets(allocate_info))
      .transform(vkutils::store_into(descriptor_sets_))
      .transform(
        [ this ]() -> void
        {
          for (auto i : std::views::iota(
                 0UZ, static_cast<std::size_t>(max_frames_in_flight)))
          {
            vk::DescriptorBufferInfo buffer_info {
              .buffer = uniform_buffers_[ i ],
              .offset = 0,
              .range = sizeof(uniform_buffer_object),
            };
            vk::DescriptorImageInfo image_info {
              .sampler = texture_sampler_,
              .imageView = texture_image_view_,
              .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            };
            std::array descriptor_writes {
              vk::WriteDescriptorSet {
                .dstSet = descriptor_sets_[ i ],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &buffer_info,
              },
              vk::WriteDescriptorSet {
                .dstSet = descriptor_sets_[ i ],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &image_info,
              },
            };
            device_.updateDescriptorSets(descriptor_writes, {});
          }
        });
  }

  constexpr auto
  create_command_buffers() -> std::expected<void, vkutils::error>
  {
    vk::CommandBufferAllocateInfo command_buffer_allocate_info {
      .commandPool = command_pool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = max_frames_in_flight,
    };

    return vkutils::locate(
      device_.allocateCommandBuffers(command_buffer_allocate_info))
      .transform(vkutils::store_into(command_buffers_));
  }

  constexpr auto
  record_command_buffer(std::uint32_t image_index)
    -> std::expected<void, vkutils::error>
  {
    const auto& command_buffer = command_buffers_[ frame_index_ ];
    if (const auto result = command_buffer.begin({}); !result.has_value())
    {
      return vkutils::locate(result);
    }

    transition_image_layout(swapchain_images_[ image_index ],
      vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
      vk::AccessFlagBits2 {}, vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::ImageAspectFlagBits::eColor);

    transition_image_layout(*color_image_, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::ImageAspectFlagBits::eColor);

    transition_image_layout(*depth_image_, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eDepthAttachmentOptimal,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests |
        vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests |
        vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue clear_color = vk::ClearColorValue { 0.0F, 0.0F, 0.0F, 1.0F };
    vk::ClearValue clear_depth = vk::ClearDepthStencilValue {
      .depth = 1.0F,
      .stencil = 0U,
    };
    vk::RenderingAttachmentInfo color_attachment_info {
      .imageView = color_image_view_,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .resolveMode = vk::ResolveModeFlagBits::eAverage,
      .resolveImageView = swapchain_image_views_[ image_index ],
      .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clear_color,
    };
    vk::RenderingAttachmentInfo depth_attachment_info {
      .imageView = depth_image_view_,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clear_depth,
    };

    vk::RenderingInfo rendering_info {
      .renderArea {
        .offset { .x = 0, .y = 0 },
        .extent = swapchain_extent_,
      },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
      .pDepthAttachment = &depth_attachment_info,
    };

    command_buffer.beginRendering(rendering_info);
    command_buffer.bindPipeline(
      vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    command_buffer.bindVertexBuffers(0, *vertex_buffer_, { 0 });
    command_buffer.bindIndexBuffer(*index_buffer_, 0, vk::IndexType::eUint32);
    command_buffer.setViewport(0,
      vk::Viewport {
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(swapchain_extent_.width),
        .height = static_cast<float>(swapchain_extent_.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
      });
    command_buffer.setScissor(0,
      vk::Rect2D {
        .offset { .x = 0, .y = 0 },
        .extent = swapchain_extent_,
      });
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
      pipeline_layout_, 0, *descriptor_sets_[ frame_index_ ], nullptr);
    command_buffer.drawIndexed(
      static_cast<std::uint32_t>(indices_.size()), 1, 0, 0, 0);
    command_buffer.endRendering();

    transition_image_layout(swapchain_images_[ image_index ],
      vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2 {},
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eBottomOfPipe,
      vk::ImageAspectFlagBits::eColor);

    return vkutils::locate(command_buffer.end());
  }

  constexpr auto
  create_sync_objects() -> std::expected<void, vkutils::error>
  {
    assert(presentation_complete_semaphores_.empty() &&
      render_finished_semaphores_.empty() && in_flight_fences_.empty());

    for (auto _ : std::views::iota(0UZ, swapchain_images_.size()))
    {
      auto sem = vkutils::locate(device_.createSemaphore({}));
      if (!sem) [[unlikely]] { return std::unexpected { sem.error() }; }
      render_finished_semaphores_.emplace_back(std::move(*sem));
    }

    for (auto _ : std::views::iota(0, max_frames_in_flight))
    {
      auto sem = vkutils::locate(device_.createSemaphore({}));
      if (!sem) [[unlikely]] { return std::unexpected { sem.error() }; }
      presentation_complete_semaphores_.emplace_back(std::move(*sem));

      auto fence = vkutils::locate(
        device_.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled }));
      if (!fence) [[unlikely]] { return std::unexpected { fence.error() }; }
      in_flight_fences_.emplace_back(std::move(*fence));
    }

    return {};
  }

  constexpr auto
  draw_frame() -> std::expected<void, vkutils::error>
  {
    const auto fence_result =
      device_.waitForFences(*in_flight_fences_[ frame_index_ ], vk::True,
        std::numeric_limits<std::uint64_t>::max());
    if (fence_result != vk::Result::eSuccess)
    {
      return std::unexpected {
        vkutils::error {
          .reason = apputils::error::wait_for_fences_failed,
        },
      };
    }

    const auto [ acquire_result, image_index ] =
      swapchain_.acquireNextImage(std::numeric_limits<std::uint64_t>::max(),
        *presentation_complete_semaphores_[ frame_index_ ], nullptr);

    if (acquire_result == vk::Result::eErrorOutOfDateKHR)
    {
      return recreate_swapchain();
    }

    if (acquire_result != vk::Result::eSuccess &&
      acquire_result != vk::Result::eSuboptimalKHR)
    {
      assert(acquire_result == vk::Result::eTimeout ||
        acquire_result == vk::Result::eNotReady);

      return std::unexpected {
        vkutils::error {
          .reason = apputils::error::next_image_acquire_failed,
        },
      };
    }

    update_uniform_buffer(frame_index_);

    return vkutils::locate(
      device_.resetFences(*in_flight_fences_[ frame_index_ ]))
      .and_then([ this ]() noexcept -> std::expected<void, vkutils::error>
        { return vkutils::locate(command_buffers_[ frame_index_ ].reset()); })
      .and_then(
        [ this, &image_index ]() noexcept -> std::expected<void, vkutils::error>
        { return record_command_buffer(image_index); })
      .and_then(
        [ this, &image_index ]() noexcept -> std::expected<void, vkutils::error>
        {
          vk::PipelineStageFlags wait_dst_stage_mask {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
          };

          const vk::SubmitInfo submit_info {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores =
              &*presentation_complete_semaphores_[ frame_index_ ],
            .pWaitDstStageMask = &wait_dst_stage_mask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*command_buffers_[ frame_index_ ],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*render_finished_semaphores_[ image_index ],
          };

          return vkutils::locate(graphics_queue_.submit(
            submit_info, *in_flight_fences_[ frame_index_ ]));
        })
      .and_then(
        [ this, &image_index ]() noexcept -> std::expected<void, vkutils::error>
        {
          const vk::PresentInfoKHR present_info_KHR {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*render_finished_semaphores_[ image_index ],
            .swapchainCount = 1,
            .pSwapchains = &*swapchain_,
            .pImageIndices = &image_index,
            .pResults = nullptr,
          };

          const auto presentation_result =
            graphics_queue_.presentKHR(present_info_KHR);

          if ((presentation_result == vk::Result::eSuboptimalKHR) ||
            (presentation_result == vk::Result::eErrorOutOfDateKHR) ||
            framebuffer_resized_)
          {
            framebuffer_resized_ = false;
            return recreate_swapchain();
          }

          if (presentation_result != vk::Result::eSuccess)
          {
            return std::unexpected {
              vkutils::error {
                .reason = apputils::error::queue_present_failed,
              },
            };
          }

          frame_index_ = (frame_index_ + 1) % max_frames_in_flight;
          return {};
        });
  }

  //  https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/04_Swap_chain_recreation.html
  //  says: The disadvantage of this approach is that we need to stop all
  //  renderings before creating the new swap chain. It is possible to create a
  //  new swap chain while drawing commands on an image from the old swap chain
  //  are still in-flight. You need to pass the previous swap chain to the
  //  oldSwapchain field in the VkSwapchainCreateInfoKHR struct and destroy the
  //  old swap chain as soon as you’ve finished using it.
  constexpr auto
  recreate_swapchain() -> std::expected<void, vkutils::error>
  {
    std::int32_t width {};
    std::int32_t height {};
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0)
    {
      glfwGetFramebufferSize(window_, &width, &height);
      glfwWaitEvents();
    }

    return vkutils::locate(device_.waitIdle())
      .and_then(
        [ this ]() -> std::expected<void, vkutils::error>
        {
          cleanup_swapchain();
          return create_swapchain();
        })
      .and_then([ this ]() -> std::expected<void, vkutils::error>
        { return create_image_views(); })
      .and_then([ this ]() -> std::expected<void, vkutils::error>
        { return create_color_resources(); })
      .and_then([ this ]() -> std::expected<void, vkutils::error>
        { return create_depth_resources(); });
  }

  constexpr void
  cleanup_swapchain()
  {
    swapchain_image_views_.clear();
    swapchain_ = nullptr;
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

    const vk::InstanceCreateInfo instance_info {
      .pApplicationInfo = &app_info,
      .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
    };

    return vkutils::locate(context_.createInstance(instance_info))
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
    std::println(std::cerr, "validation layer: type {}\nmsg: {}",
      vk::to_string(type), p_callback_data->pMessage);

    return vk::False;
  }

  constexpr auto
  is_device_suitable(const vk::raii::PhysicalDevice& device) -> bool
  {
    auto supported_features = device.getFeatures();

    return has_minimum_api_version(device) &&
      has_graphics_queue_family(device) && has_required_extensions(device) &&
      (supported_features.samplerAnisotropy != 0U);
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
  create_swapchain(const vk::SurfaceCapabilitiesKHR& surface_capabilities,
    std::span<const vk::PresentModeKHR> surface_presentation_modes)
    -> std::expected<void, vkutils::error>
  {
    const vk::SwapchainCreateInfoKHR swapchain_info {
      .flags = vk::SwapchainCreateFlagsKHR(),
      .surface = *surface_,
      .minImageCount = choose_swap_min_image_count(surface_capabilities),
      .imageFormat = swapchain_surface_format_.format,
      .imageColorSpace = swapchain_surface_format_.colorSpace,
      .imageExtent = swapchain_extent_,
      .imageArrayLayers = 1U,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surface_capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = choose_swap_presentation_mode(surface_presentation_modes),
      .clipped = vk::True,
      .oldSwapchain = nullptr,
    };

    return create_swapchain_impl(swapchain_info)
      .and_then([ this ]() -> std::expected<void, vkutils::error>
        { return get_swapchain_images(); });
  }

  constexpr auto
  create_swapchain_impl(const vk::SwapchainCreateInfoKHR& swapchain_info)
    -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(device_.createSwapchainKHR(swapchain_info))
      .transform(vkutils::store_into(swapchain_));
  }

  constexpr auto
  get_swapchain_images() -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(swapchain_.getImages())
      .transform(vkutils::store_into(swapchain_images_));
  }

  [[nodiscard]]
  constexpr auto
  create_shader_module(std::span<const char> code)
    -> std::expected<vk::raii::ShaderModule, vkutils::error>
  {
    vk::ShaderModuleCreateInfo info {
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const std::uint32_t*>(code.data()),
    };

    return vkutils::locate(device_.createShaderModule(info));
  }

  constexpr void
  transition_image_layout(vk::Image image, vk::ImageLayout old_layout,
    vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
    vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_pipeline_stage_mask,
    vk::PipelineStageFlags2 dst_pipeline_stage_mask,
    vk::ImageAspectFlags image_aspect_flags)
  {
    vk::ImageMemoryBarrier2 barrier {
      .srcStageMask = src_pipeline_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_pipeline_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange {
        .aspectMask = image_aspect_flags,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    vk::DependencyInfo dependency_info {
      .dependencyFlags = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
    };

    command_buffers_[ frame_index_ ].pipelineBarrier2(dependency_info);
  }

  constexpr auto
  transition_image_layout(const vk::raii::Image& image,
    vk::ImageLayout old_layout, vk::ImageLayout new_layout,
    std::uint32_t mip_levels) -> std::expected<void, vkutils::error>
  {
    return begin_single_time_commands().and_then(
      [ &, this ](vk::raii::CommandBuffer&& command_copy_buffer) noexcept
        -> std::expected<void, vkutils::error>
      {
        vk::ImageMemoryBarrier barrier {
          .oldLayout = old_layout,
          .newLayout = new_layout,
          .image = image,
          .subresourceRange {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = 1,
          },
        };

        vk::PipelineStageFlags source_stage {};
        vk::PipelineStageFlags destination_stage {};

        if (old_layout == vk::ImageLayout::eUndefined &&
          new_layout == vk::ImageLayout::eTransferDstOptimal)
        {
          barrier.srcAccessMask = {};
          barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
          source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
          destination_stage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
          new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {

          barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
          barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
          source_stage = vk::PipelineStageFlagBits::eTransfer;
          destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
          return std::unexpected {
            vkutils::error {
              .reason = apputils::error::unsupported_layout_transition,
            },
          };
        }

        command_copy_buffer.pipelineBarrier(
          source_stage, destination_stage, {}, {}, nullptr, barrier);

        return end_single_time_commands(std::move(command_copy_buffer));
      });
  }

  constexpr auto
  create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer,
    vk::raii::DeviceMemory& buffer_memory)
    -> std::expected<void, vkutils::error>
  {
    vk::BufferCreateInfo buffer_info {
      .size = size,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive,
    };

    return vkutils::locate(device_.createBuffer(buffer_info))
      .transform(vkutils::store_into(buffer))
      .and_then(
        [ & ]() noexcept -> std::expected<void, vkutils::error>
        {
          vk::MemoryRequirements requirements = buffer.getMemoryRequirements();
          return allocate_memory(buffer_memory, requirements, properties);
        })
      .and_then([ & ]() noexcept -> std::expected<void, vkutils::error>
        { return vkutils::locate(buffer.bindMemory(*buffer_memory, 0)); });
  }

  constexpr auto
  find_memory_type(
    std::uint32_t type_filter, vk::MemoryPropertyFlags properties)
    -> std::expected<std::uint32_t, vkutils::error>
  {
    vk::PhysicalDeviceMemoryProperties memory_properties =
      physical_device_.getMemoryProperties();

    for (auto i : std::views::iota(0U, memory_properties.memoryTypeCount))
    {
      if (((type_filter & (1 << i)) != 0U) &&
        (memory_properties.memoryTypes[ i ].propertyFlags & properties) ==
          properties)
      {
        return i;
      }
    }

    return std::unexpected {
      vkutils::error {
        .reason = apputils::error::search_for_memory_type_failed,
      },
    };
  }

  constexpr auto
  allocate_memory(vk::raii::DeviceMemory& memory,
    vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties)
    -> std::expected<void, vkutils::error>
  {
    return find_memory_type(requirements.memoryTypeBits, properties)
      .and_then(
        [ &, this ](std::uint32_t memory_type) noexcept
          -> std::expected<void, vkutils::error>
        {
          vk::MemoryAllocateInfo memory_allocate_info {
            .allocationSize = requirements.size,
            .memoryTypeIndex = memory_type,
          };
          return vkutils::locate(device_.allocateMemory(memory_allocate_info))
            .transform(vkutils::store_into(memory));
        });
  }

  constexpr auto
  map_memory(vk::DeviceSize size, vk::raii::DeviceMemory& memory,
    std::ranges::range auto&& src_data) -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(memory.mapMemory(0, size))
      .transform(
        [ & ](void* dst_data) noexcept -> void
        {
          std::memcpy(dst_data, src_data.data(), size);
          memory.unmapMemory();
        });
  }

  // https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/02_Staging_buffer.html
  // says: You may wish to create a separate command pool for these kinds of
  // short-lived buffers, because the implementation may be able to apply memory
  // allocation optimizations. You should use the
  // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in
  // that case.
  constexpr auto
  copy_buffer(vk::raii::Buffer& source_buffer,
    vk::raii::Buffer& destination_buffer, vk::DeviceSize size)
    -> std::expected<void, vkutils::error>
  {
    return begin_single_time_commands()
      .transform(
        [ & ](vk::raii::CommandBuffer command_copy_buffer) noexcept
          -> vk::raii::CommandBuffer
        {
          command_copy_buffer.copyBuffer(source_buffer, destination_buffer,
            vk::BufferCopy {
              .srcOffset = 0,
              .dstOffset = 0,
              .size = size,
            });

          return command_copy_buffer;
        })
      .and_then([ this ](vk::raii::CommandBuffer command_copy_buffer) noexcept
                  -> std::expected<void, vkutils::error>
        { return end_single_time_commands(std::move(command_copy_buffer)); });
  }

  constexpr void
  update_uniform_buffer(std::uint32_t current_image)
  {
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration<float, std::chrono::seconds::period>(
      current_time - start_time)
                  .count();

    uniform_buffer_object ubo {};
    ubo.model = glm::rotate(
      glm::mat4(1.0F), time * glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
    ubo.view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F),
      glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 0.0F, 1.0F));
    ubo.proj = glm::perspective(glm::radians(45.0F),
      static_cast<float>(swapchain_extent_.width) /
        static_cast<float>(swapchain_extent_.height),
      0.1F, 10.0F);

    // Invert Y axis (OpenGL has it inverted)
    ubo.proj[ 1 ][ 1 ] *= -1;

    std::memcpy(uniform_buffers_mapped_[ current_image ], &ubo, sizeof(ubo));
  }

  constexpr auto
  create_image(std::uint32_t width, std::uint32_t height,
    std::uint32_t mip_levels, vk::SampleCountFlagBits sample_count,
    vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties, vk::raii::Image& image,
    vk::raii::DeviceMemory& image_memory) -> std::expected<void, vkutils::error>
  {
    vk::ImageCreateInfo image_info {
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent {
        .width = width,
        .height = height,
        .depth = 1,
      },
      .mipLevels = mip_levels,
      .arrayLayers = 1,
      .samples = sample_count,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive,
    };

    return vkutils::locate(device_.createImage(image_info))
      .transform(vkutils::store_into(image))
      .and_then(
        [ & ]() noexcept -> std::expected<void, vkutils::error>
        {
          vk::MemoryRequirements requirements = image.getMemoryRequirements();
          return allocate_memory(image_memory, requirements, properties);
        })
      .and_then([ & ]() noexcept -> std::expected<void, vkutils::error>
        { return vkutils::locate(image.bindMemory(*image_memory, 0)); });
  }

  // https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/00_Images.html
  // says: All the helper functions that submit commands so far have been set up
  // to execute synchronously by waiting for the queue to become idle. For
  // practical applications it is recommended to combine these operations in a
  // single command buffer and execute them asynchronously for higher
  // throughput, especially the transitions and copy in the createTextureImage
  // function. Try to experiment with this by creating a setupCommandBuffer that
  // the helper functions record commands into, and add a flushSetupCommands to
  // execute the commands that have been recorded so far. It’s best to do this
  // after the texture mapping works to check if the texture resources are still
  // set up correctly.
  constexpr auto
  begin_single_time_commands()
    -> std::expected<vk::raii::CommandBuffer, vkutils::error>
  {
    vk::CommandBufferAllocateInfo allocate_info {
      .commandPool = command_pool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
    };

    return vkutils::locate(device_.allocateCommandBuffers(allocate_info))
      .and_then(
        [](auto&& buffers)
          -> std::expected<vk::raii::CommandBuffer, vkutils::error>
        {
          auto command_buffer = std::move(buffers.front());
          return vkutils::locate(
            command_buffer.begin(
              { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit }))
            .transform([ & ]() -> vk::raii::CommandBuffer
              { return std::move(command_buffer); });
        });
  }

  constexpr auto
  end_single_time_commands(vk::raii::CommandBuffer command_buffer)
    -> std::expected<void, vkutils::error>
  {
    return vkutils::locate(command_buffer.end())
      .and_then(
        [ &, this ]() noexcept -> std::expected<void, vkutils::error>
        {
          return vkutils::locate(graphics_queue_.submit(
            vk::SubmitInfo {
              .commandBufferCount = 1,
              .pCommandBuffers = &*command_buffer,
            },
            nullptr));
        })
      .and_then([ this ]() noexcept -> std::expected<void, vkutils::error>
        { return vkutils::locate(graphics_queue_.waitIdle()); });
  }

  constexpr auto
  copy_buffer_to_image(const vk::raii::Buffer& buffer, vk::raii::Image& image,
    std::uint32_t width, std::uint32_t height)
    -> std::expected<void, vkutils::error>
  {
    return begin_single_time_commands().and_then(
      [ & ](vk::raii::CommandBuffer command_copy_buffer) noexcept
        -> std::expected<void, vkutils::error>
      {
        vk::BufferImageCopy region {
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
          },
          .imageOffset {
            .x = 0,
            .y = 0,
            .z = 0,
          },
          .imageExtent {
            .width = width,
            .height = height,
            .depth = 1,
          },
        };
        command_copy_buffer.copyBufferToImage(
          buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });

        return end_single_time_commands(std::move(command_copy_buffer));
      });
  }

  constexpr auto
  create_image_view(vk::Image image, vk::Format format,
    vk::ImageAspectFlags aspect_flags, std::uint32_t mip_levels)
    -> std::expected<vk::raii::ImageView, vkutils::error>
  {
    vk::ImageViewCreateInfo view_info {
      .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange {
        .aspectMask = aspect_flags,
        .baseMipLevel = 0,
        .levelCount = mip_levels,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    return vkutils::locate(device_.createImageView(view_info));
  }

  constexpr auto
  find_supported_format(std::span<const vk::Format> candidates,
    vk::ImageTiling tiling, vk::FormatFeatureFlags features)
    -> std::expected<vk::Format, vkutils::error>
  {
    for (const auto format : candidates)
    {
      const auto properties = physical_device_.getFormatProperties(format);

      if (tiling == vk::ImageTiling::eLinear &&
        (properties.linearTilingFeatures & features) == features)
      {
        return format;
      }

      if (tiling == vk::ImageTiling::eOptimal &&
        (properties.optimalTilingFeatures & features) == features)
      {
        return format;
      }
    }

    return std::unexpected {
      vkutils::error {
        .reason = apputils::error::search_for_supported_format_failed,
      },
    };
  }

  constexpr auto
  find_depth_format() -> std::expected<vk::Format, vkutils::error>
  {
    return find_supported_format(
      { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint },
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
  }

  constexpr auto
  has_stencil_component(vk::Format format) -> bool
  {
    return format == vk::Format::eD32SfloatS8Uint ||
      format == vk::Format::eD24UnormS8Uint;
  }

  // TODO(Konrad): its a mess - so fix the mess
  constexpr auto
  generate_mip_maps(vk::raii::Image& image, vk::Format image_format,
    std::int32_t texture_width, std::int32_t texture_height,
    std::uint32_t mip_levels) -> std::expected<void, vkutils::error>
  {
    const auto format_properties =
      physical_device_.getFormatProperties(image_format);
    if (!(format_properties.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
    {
      return std::unexpected {
        vkutils::error {
          .reason = apputils::error::search_for_supported_format_failed,
        },
      };
    }

    return begin_single_time_commands().and_then(
      [ & ](vk::raii::CommandBuffer&& command_buffer) noexcept
        -> std::expected<void, vkutils::error>
      {
        vk::ImageMemoryBarrier barrier {
          .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
          .dstAccessMask = vk::AccessFlagBits::eTransferRead,
          .oldLayout = vk::ImageLayout::eTransferDstOptimal,
          .newLayout = vk::ImageLayout::eTransferSrcOptimal,
          .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
          .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
          .image = image,
          .subresourceRange {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
          },
        };

        std::int32_t mip_width = texture_width;
        std::int32_t mip_height = texture_height;
        for (auto mip_level : std::views::iota(1U, mip_levels))
        {
          barrier.subresourceRange.baseMipLevel = mip_level - 1U;
          barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
          barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
          barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
          barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

          command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

          const std::array source_offsets {
            vk::Offset3D { .x = 0, .y = 0, .z = 0 },
            vk::Offset3D { .x = mip_width, .y = mip_height, .z = 1 },
          };
          const std::array destination_offsets {
            vk::Offset3D { .x = 0, .y = 0, .z = 0 },
            vk::Offset3D {
              .x = mip_width > 1 ? mip_width / 2 : 1,
              .y = mip_height > 1 ? mip_height / 2 : 1,
              .z = 1,
            },
          };

          vk::ImageBlit blit {
            .srcSubresource = {},
            .srcOffsets = source_offsets,
            .dstSubresource = {},
            .dstOffsets = destination_offsets,
          };
          blit.srcSubresource = vk::ImageSubresourceLayers {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = mip_level - 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
          };
          blit.dstSubresource = vk::ImageSubresourceLayers {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = mip_level,
            .baseArrayLayer = 0,
            .layerCount = 1,
          };
          command_buffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
            image, vk::ImageLayout::eTransferDstOptimal, { blit },
            vk::Filter::eLinear);

          barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
          barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
          barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
          barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
          command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

          if (mip_width > 1) { mip_width /= 2; }
          if (mip_height > 1) { mip_height /= 2; }
        }

        barrier.subresourceRange.baseMipLevel = mip_levels - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
          vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        return end_single_time_commands(std::move(command_buffer));
      });
  }

  constexpr auto
  get_max_usable_sample_count() -> vk::SampleCountFlagBits
  {
    const auto physical_device_properties = physical_device_.getProperties();

    auto counts =
      physical_device_properties.limits.framebufferColorSampleCounts &
      physical_device_properties.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64)
    {
      return vk::SampleCountFlagBits::e64;
    }
    if (counts & vk::SampleCountFlagBits::e32)
    {
      return vk::SampleCountFlagBits::e32;
    }
    if (counts & vk::SampleCountFlagBits::e16)
    {
      return vk::SampleCountFlagBits::e16;
    }
    if (counts & vk::SampleCountFlagBits::e8)
    {
      return vk::SampleCountFlagBits::e8;
    }
    if (counts & vk::SampleCountFlagBits::e4)
    {
      return vk::SampleCountFlagBits::e4;
    }
    if (counts & vk::SampleCountFlagBits::e2)
    {
      return vk::SampleCountFlagBits::e2;
    }

    return vk::SampleCountFlagBits::e1;
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
  vk::raii::SwapchainKHR swapchain_ { nullptr };
  std::vector<vk::Image> swapchain_images_;
  std::vector<vk::raii::ImageView> swapchain_image_views_;
  vk::raii::DescriptorSetLayout descriptor_set_layout_ { nullptr };
  vk::raii::PipelineLayout pipeline_layout_ { nullptr };
  vk::raii::Pipeline graphics_pipeline_ { nullptr };
  vk::raii::CommandPool command_pool_ { nullptr };
  vk::raii::DescriptorPool descriptor_pool_ { nullptr };
  std::vector<vk::raii::DescriptorSet> descriptor_sets_;

  std::vector<vertex> vertices_;
  std::vector<std::uint32_t> indices_;
  vk::raii::Buffer vertex_buffer_ { nullptr };
  vk::raii::DeviceMemory vertex_buffer_memory_ { nullptr };
  vk::raii::Buffer index_buffer_ { nullptr };
  vk::raii::DeviceMemory index_buffer_memory_ { nullptr };

  std::vector<vk::raii::Buffer> uniform_buffers_;
  std::vector<vk::raii::DeviceMemory> uniform_buffers_memory_;
  std::vector<void*> uniform_buffers_mapped_;

  std::vector<vk::raii::CommandBuffer> command_buffers_;
  std::vector<vk::raii::Semaphore> presentation_complete_semaphores_;
  std::vector<vk::raii::Semaphore> render_finished_semaphores_;
  std::vector<vk::raii::Fence> in_flight_fences_;

  vk::raii::Image texture_image_ { nullptr };
  vk::raii::DeviceMemory texture_image_memory_ { nullptr };
  vk::raii::ImageView texture_image_view_ { nullptr };
  vk::raii::Sampler texture_sampler_ { nullptr };

  vk::raii::Image depth_image_ { nullptr };
  vk::raii::DeviceMemory depth_image_memory_ { nullptr };
  vk::raii::ImageView depth_image_view_ { nullptr };

  vk::raii::Image color_image_ { nullptr };
  vk::raii::DeviceMemory color_image_memory_ { nullptr };
  vk::raii::ImageView color_image_view_ { nullptr };

  // 4 bytes alignment
  vk::SurfaceFormatKHR swapchain_surface_format_ {};
  vk::Extent2D swapchain_extent_ {};
  std::uint32_t graphics_queue_index_ { ~0U };
  std::uint32_t presentation_queue_index_ { ~0U };
  std::uint32_t frame_index_ {};

  vk::Format depth_format_ {};

  std::uint32_t mip_levels_ {};

  vk::SampleCountFlagBits msaa_samples_ = vk::SampleCountFlagBits::e1;

  // 1 byte alignment
  bool framebuffer_resized_ {};
};