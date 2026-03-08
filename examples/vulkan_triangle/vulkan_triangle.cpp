#include <assert.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const*> validation_layers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enable_validation_layers = false;
#else
constexpr bool enable_validation_layers = true;
#endif

class TriangleApplication {
 public:
  void run() {
    init_window();
    init_vulkan();
    main_loop();
    cleanup();
  }

 private:
  GLFWwindow* window = nullptr;
  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;
  vk::raii::PhysicalDevice physical_device = nullptr;
  vk::raii::Device device = nullptr;
  uint32_t queue_index = ~0;
  vk::raii::Queue queue = nullptr;
  vk::raii::SwapchainKHR swap_chain = nullptr;
  std::vector<vk::Image> swap_chain_images;
  vk::SurfaceFormatKHR swap_chain_surface_format;
  vk::Extent2D swap_chain_extent;
  std::vector<vk::raii::ImageView> swap_chain_image_views;

  vk::raii::PipelineLayout pipeline_layout = nullptr;
  vk::raii::Pipeline graphics_pipeline = nullptr;

  vk::raii::Buffer vertex_buffer = nullptr;
  vk::raii::DeviceMemory vertex_buffer_memory = nullptr;

  vk::raii::CommandPool command_pool = nullptr;
  vk::raii::CommandBuffer command_buffer = nullptr;

  vk::raii::Semaphore present_complete_semaphore = nullptr;
  vk::raii::Semaphore render_finished_semaphore = nullptr;
  vk::raii::Fence draw_fence = nullptr;

  std::vector<const char*> required_device_extension = {
      vk::KHRSwapchainExtensionName};

  void init_window() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Triangle", nullptr, nullptr);
  }

  void init_vulkan() {
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swap_chain();
    create_image_views();
    create_graphics_pipeline();
    create_vertex_buffer();
    create_command_pool();
    create_command_buffer();
    create_sync_objects();
  }

  void main_loop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      draw_frame();
    }
    device.waitIdle();
  }

  void cleanup() {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  void create_instance() {
    vk::ApplicationInfo app_info = {};
    app_info.pApplicationName = "Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = vk::ApiVersion14;

    std::vector<char const*> required_layers;
    if (enable_validation_layers) {
      required_layers.assign(validation_layers.begin(),
                             validation_layers.end());
    }

    auto layer_properties = context.enumerateInstanceLayerProperties();
    auto unsupported_layer_it = std::ranges::find_if(
        required_layers, [&layer_properties](auto const& required_layer) {
          return std::ranges::none_of(
              layer_properties, [required_layer](auto const& layer_property) {
                return strcmp(layer_property.layerName, required_layer) == 0;
              });
        });
    if (unsupported_layer_it != required_layers.end()) {
      throw std::runtime_error("Required layer not supported: " +
                               std::string(*unsupported_layer_it));
    }

    auto required_extensions = get_required_instance_extensions();

    auto extension_properties = context.enumerateInstanceExtensionProperties();
    auto unsupported_property_it = std::ranges::find_if(
        required_extensions,
        [&extension_properties](auto const& required_extension) {
          return std::ranges::none_of(
              extension_properties,
              [required_extension](auto const& extension_property) {
                return strcmp(extension_property.extensionName,
                              required_extension) == 0;
              });
        });
    if (unsupported_property_it != required_extensions.end()) {
      throw std::runtime_error("Required extension not supported: " +
                               std::string(*unsupported_property_it));
    }

    vk::InstanceCreateInfo create_info = {};
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount =
        static_cast<uint32_t>(required_layers.size());
    create_info.ppEnabledLayerNames = required_layers.data();
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    instance = vk::raii::Instance(context, create_info);
  }

  void setup_debug_messenger() {
    if (!enable_validation_layers) return;

    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info = {};
    debug_utils_messenger_create_info.messageSeverity = severity_flags;
    debug_utils_messenger_create_info.messageType = message_type_flags;
    debug_utils_messenger_create_info.pfnUserCallback = &debug_callback;
    debug_messenger = instance.createDebugUtilsMessengerEXT(
        debug_utils_messenger_create_info);
  }

  void create_surface() {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  bool is_device_suitable(vk::raii::PhysicalDevice const& physical_device) {
    bool supports_vulkan_1_3 =
        physical_device.getProperties().apiVersion >= VK_API_VERSION_1_3;

    auto queue_families = physical_device.getQueueFamilyProperties();
    bool supports_graphics =
        std::ranges::any_of(queue_families, [](auto const& qfp) {
          return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
        });

    auto available_device_extensions =
        physical_device.enumerateDeviceExtensionProperties();
    bool supports_all_required_extensions = std::ranges::all_of(
        required_device_extension,
        [&available_device_extensions](auto const& required_ext) {
          return std::ranges::any_of(
              available_device_extensions,
              [required_ext](auto const& available_ext) {
                return strcmp(available_ext.extensionName, required_ext) == 0;
              });
        });

    auto features = physical_device.template getFeatures2<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supports_required_features =
        features.template get<vk::PhysicalDeviceVulkan11Features>()
            .shaderDrawParameters &&
        features.template get<vk::PhysicalDeviceVulkan13Features>()
            .dynamicRendering &&
        features.template get<vk::PhysicalDeviceVulkan13Features>()
            .synchronization2 &&
        features
            .template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
            .extendedDynamicState;

    return supports_vulkan_1_3 && supports_graphics &&
           supports_all_required_extensions && supports_required_features;
  }

  void pick_physical_device() {
    std::vector<vk::raii::PhysicalDevice> physical_devices =
        instance.enumeratePhysicalDevices();
    auto const dev_iter = std::ranges::find_if(
        physical_devices,
        [&](auto const& pd) { return is_device_suitable(pd); });
    if (dev_iter == physical_devices.end()) {
      throw std::runtime_error("failed to find a suitable GPU!");
    }
    physical_device = *dev_iter;
  }

  void create_logical_device() {
    std::vector<vk::QueueFamilyProperties> queue_family_properties =
        physical_device.getQueueFamilyProperties();

    for (uint32_t qfp_index = 0; qfp_index < queue_family_properties.size();
         qfp_index++) {
      if ((queue_family_properties[qfp_index].queueFlags &
           vk::QueueFlagBits::eGraphics) &&
          physical_device.getSurfaceSupportKHR(qfp_index, *surface)) {
        queue_index = qfp_index;
        break;
      }
    }
    if (queue_index == ~0) {
      throw std::runtime_error(
          "Could not find a queue for graphics and present -> terminating");
    }

    vk::PhysicalDeviceFeatures2 physical_device_features2 = {};

    vk::PhysicalDeviceVulkan11Features physical_device_vulkan11_features = {};
    physical_device_vulkan11_features.shaderDrawParameters = true;

    vk::PhysicalDeviceVulkan13Features physical_device_vulkan13_features = {};
    physical_device_vulkan13_features.synchronization2 = true;
    physical_device_vulkan13_features.dynamicRendering = true;

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        physical_device_extended_dynamic_state_features = {};
    physical_device_extended_dynamic_state_features.extendedDynamicState = true;

    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan11Features,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        feature_chain = {physical_device_features2,
                         physical_device_vulkan11_features,
                         physical_device_vulkan13_features,
                         physical_device_extended_dynamic_state_features};

    float queue_priority = 0.5f;
    vk::DeviceQueueCreateInfo device_queue_create_info = {};
    device_queue_create_info.queueFamilyIndex = queue_index;
    device_queue_create_info.queueCount = 1;
    device_queue_create_info.pQueuePriorities = &queue_priority;
    vk::DeviceCreateInfo device_create_info = {};
    device_create_info.pNext =
        &feature_chain.get<vk::PhysicalDeviceFeatures2>();
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &device_queue_create_info;
    device_create_info.enabledExtensionCount =
        static_cast<uint32_t>(required_device_extension.size());
    device_create_info.ppEnabledExtensionNames =
        required_device_extension.data();

    device = vk::raii::Device(physical_device, device_create_info);
    queue = vk::raii::Queue(device, queue_index, 0);
  }

  void create_swap_chain() {
    vk::SurfaceCapabilitiesKHR surface_capabilities =
        physical_device.getSurfaceCapabilitiesKHR(*surface);
    swap_chain_extent = choose_swap_extent(surface_capabilities);
    uint32_t min_image_count =
        choose_swap_min_image_count(surface_capabilities);

    std::vector<vk::SurfaceFormatKHR> available_formats =
        physical_device.getSurfaceFormatsKHR(*surface);
    swap_chain_surface_format = choose_swap_surface_format(available_formats);

    std::vector<vk::PresentModeKHR> available_present_modes =
        physical_device.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR present_mode =
        choose_swap_present_mode(available_present_modes);

    vk::SwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.surface = *surface;
    swap_chain_create_info.minImageCount = min_image_count;
    swap_chain_create_info.imageFormat = swap_chain_surface_format.format;
    swap_chain_create_info.imageColorSpace =
        swap_chain_surface_format.colorSpace;
    swap_chain_create_info.imageExtent = swap_chain_extent;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage =
        vk::ImageUsageFlagBits::eColorAttachment;
    swap_chain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
    swap_chain_create_info.preTransform = surface_capabilities.currentTransform;
    swap_chain_create_info.compositeAlpha =
        vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swap_chain_create_info.presentMode = present_mode;
    swap_chain_create_info.clipped = true;

    swap_chain = vk::raii::SwapchainKHR(device, swap_chain_create_info);
    swap_chain_images = swap_chain.getImages();
  }

  void create_image_views() {
    assert(swap_chain_image_views.empty());

    vk::ImageViewCreateInfo image_view_create_info = {};
    image_view_create_info.viewType = vk::ImageViewType::e2D;
    image_view_create_info.format = swap_chain_surface_format.format;
    image_view_create_info.subresourceRange = {vk::ImageAspectFlagBits::eColor,
                                               0, 1, 0, 1};
    for (auto& image : swap_chain_images) {
      image_view_create_info.image = image;
      swap_chain_image_views.emplace_back(device, image_view_create_info);
    }
  }

  void create_graphics_pipeline() {
    vk::raii::ShaderModule vert_shader_module =
        create_shader_module(read_file("shaders/vertex.spv"));
    vk::raii::ShaderModule frag_shader_module =
        create_shader_module(read_file("shaders/fragment.spv"));

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info, frag_shader_stage_info};

    vk::VertexInputBindingDescription binding_description = {};
    binding_description.binding = 0;
    binding_description.stride = sizeof(float) * 3;
    binding_description.inputRate = vk::VertexInputRate::eVertex;

    vk::VertexInputAttributeDescription attribute_description = {};
    attribute_description.binding = 0;
    attribute_description.location = 0;
    attribute_description.format = vk::Format::eR32G32B32Sfloat;
    attribute_description.offset = 0;

    vk::PipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = 1;
    vertex_input_info.pVertexAttributeDescriptions = &attribute_description;

    vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    vk::PipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.depthBiasSlopeFactor = 1.0f;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.blendEnable = vk::False;
    color_blend_attachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.logicOpEnable = vk::False;
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    std::vector dynamic_states = {vk::DynamicState::eViewport,
                                  vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.dynamicStateCount =
        static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    vk::PipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pushConstantRangeCount = 0;

    pipeline_layout = vk::raii::PipelineLayout(device, pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
    graphics_pipeline_create_info.stageCount = 2;
    graphics_pipeline_create_info.pStages = shader_stages;
    graphics_pipeline_create_info.pVertexInputState = &vertex_input_info;
    graphics_pipeline_create_info.pInputAssemblyState = &input_assembly;
    graphics_pipeline_create_info.pViewportState = &viewport_state;
    graphics_pipeline_create_info.pRasterizationState = &rasterizer;
    graphics_pipeline_create_info.pMultisampleState = &multisampling;
    graphics_pipeline_create_info.pColorBlendState = &color_blending;
    graphics_pipeline_create_info.pDynamicState = &dynamic_state;
    graphics_pipeline_create_info.layout = pipeline_layout;
    graphics_pipeline_create_info.renderPass = nullptr;

    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info = {};
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    pipeline_rendering_create_info.pColorAttachmentFormats =
        &swap_chain_surface_format.format;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                       vk::PipelineRenderingCreateInfo>
        pipeline_create_info_chain = {graphics_pipeline_create_info,
                                      pipeline_rendering_create_info};

    graphics_pipeline = vk::raii::Pipeline(
        device, nullptr,
        pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());
  }

  void create_vertex_buffer() {
    const std::vector<float> vertices = {
        0.0f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f, -0.5f, 0.5f, 0.0f,
    };
    vk::DeviceSize buffer_size = sizeof(float) * vertices.size();

    vk::BufferCreateInfo buffer_info = {};
    buffer_info.size = buffer_size;
    buffer_info.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;
    vertex_buffer = vk::raii::Buffer(device, buffer_info);

    vk::MemoryRequirements mem_requirements =
        vertex_buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info = {};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex =
        find_memory_type(mem_requirements.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eHostVisible |
                             vk::MemoryPropertyFlagBits::eHostCoherent);
    vertex_buffer_memory = vk::raii::DeviceMemory(device, alloc_info);
    vertex_buffer.bindMemory(*vertex_buffer_memory, 0);

    void* data = vertex_buffer_memory.mapMemory(0, buffer_size);
    memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    vertex_buffer_memory.unmapMemory();
  }

  uint32_t find_memory_type(uint32_t type_filter,
                            vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties mem_properties =
        physical_device.getMemoryProperties();
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
      if ((type_filter & (1 << i)) &&
          (mem_properties.memoryTypes[i].propertyFlags & properties) ==
              properties) {
        return i;
      }
    }
    throw std::runtime_error("failed to find suitable memory type!");
  }

  void create_command_pool() {
    vk::CommandPoolCreateInfo pool_info = {};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = queue_index;
    command_pool = vk::raii::CommandPool(device, pool_info);
  }

  void create_command_buffer() {
    vk::CommandBufferAllocateInfo alloc_info = {};
    alloc_info.commandPool = command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;
    command_buffer =
        std::move(vk::raii::CommandBuffers(device, alloc_info).front());
  }

  void record_command_buffer(uint32_t image_index) {
    command_buffer.begin({});
    transition_image_layout(image_index, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eColorAttachmentOptimal, {},
                            vk::AccessFlagBits2::eColorAttachmentWrite,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    vk::ClearValue clear_color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo attachment_info = {};
    attachment_info.imageView = swap_chain_image_views[image_index];
    attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    attachment_info.clearValue = clear_color;
    vk::RenderingInfo rendering_info = {};
    rendering_info.renderArea = {.offset = {0, 0}, .extent = swap_chain_extent};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &attachment_info;

    command_buffer.beginRendering(rendering_info);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                *graphics_pipeline);
    command_buffer.bindVertexBuffers(0, {*vertex_buffer}, {0});
    command_buffer.setViewport(
        0,
        vk::Viewport(0.0f, 0.0f, static_cast<float>(swap_chain_extent.width),
                     static_cast<float>(swap_chain_extent.height), 0.0f, 1.0f));
    command_buffer.setScissor(
        0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_extent));
    command_buffer.draw(3, 1, 0, 0);
    command_buffer.endRendering();
    transition_image_layout(image_index,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::ePresentSrcKHR,
                            vk::AccessFlagBits2::eColorAttachmentWrite, {},
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eBottomOfPipe);
    command_buffer.end();
  }

  void transition_image_layout(uint32_t image_index, vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout,
                               vk::AccessFlags2 src_access_mask,
                               vk::AccessFlags2 dst_access_mask,
                               vk::PipelineStageFlags2 src_stage_mask,
                               vk::PipelineStageFlags2 dst_stage_mask) {
    vk::ImageMemoryBarrier2 barrier = {};
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swap_chain_images[image_index];
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vk::DependencyInfo dependency_info = {};
    dependency_info.dependencyFlags = {};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;
    command_buffer.pipelineBarrier2(dependency_info);
  }

  void create_sync_objects() {
    present_complete_semaphore =
        vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
    render_finished_semaphore =
        vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());

    vk::FenceCreateInfo fence_create_info = {};
    fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;
    draw_fence = vk::raii::Fence(device, fence_create_info);
  }

  void draw_frame() {
    queue.waitIdle();

    auto [result, image_index] = swap_chain.acquireNextImage(
        UINT64_MAX, *present_complete_semaphore, nullptr);
    record_command_buffer(image_index);

    device.resetFences(*draw_fence);
    vk::PipelineStageFlags wait_destination_stage_mask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submit_info = {};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &*present_complete_semaphore;
    submit_info.pWaitDstStageMask = &wait_destination_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &*command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &*render_finished_semaphore;
    queue.submit(submit_info, *draw_fence);
    result = device.waitForFences(*draw_fence, vk::True, UINT64_MAX);
    if (result != vk::Result::eSuccess) {
      throw std::runtime_error("failed to wait for fence!");
    }

    vk::PresentInfoKHR present_info = {};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &*render_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &*swap_chain;
    present_info.pImageIndices = &image_index;
    result = queue.presentKHR(present_info);
    switch (result) {
      case vk::Result::eSuccess:
        break;
      case vk::Result::eSuboptimalKHR:
        std::cout
            << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
        break;
      default:
        break;
    }
  }

  [[nodiscard]] vk::raii::ShaderModule create_shader_module(
      const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo create_info = {};
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());
    vk::raii::ShaderModule shader_module{device, create_info};

    return shader_module;
  }

  static uint32_t choose_swap_min_image_count(
      vk::SurfaceCapabilitiesKHR const& surface_capabilities) {
    auto min_image_count = std::max(3u, surface_capabilities.minImageCount);
    if ((0 < surface_capabilities.maxImageCount) &&
        (surface_capabilities.maxImageCount < min_image_count)) {
      min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
  }

  static vk::SurfaceFormatKHR choose_swap_surface_format(
      const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    assert(!available_formats.empty());
    const auto format_it =
        std::ranges::find_if(available_formats, [](const auto& format) {
          return format.format == vk::Format::eB8G8R8A8Srgb &&
                 format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
    return format_it != available_formats.end() ? *format_it
                                                : available_formats[0];
  }

  static vk::PresentModeKHR choose_swap_present_mode(
      std::vector<vk::PresentModeKHR> const& available_present_modes) {
    assert(std::ranges::any_of(available_present_modes, [](auto present_mode) {
      return present_mode == vk::PresentModeKHR::eFifo;
    }));
    return std::ranges::any_of(available_present_modes,
                               [](const vk::PresentModeKHR value) {
                                 return vk::PresentModeKHR::eMailbox == value;
                               })
               ? vk::PresentModeKHR::eMailbox
               : vk::PresentModeKHR::eFifo;
  }

  vk::Extent2D choose_swap_extent(
      vk::SurfaceCapabilitiesKHR const& capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height)};
  }

  std::vector<const char*> get_required_instance_extensions() {
    uint32_t glfw_extension_count = 0;
    auto glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector extensions(glfw_extensions,
                           glfw_extensions + glfw_extension_count);
    if (enable_validation_layers) {
      extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
  }

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data, void*) {
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
        severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
      std::cerr << "validation layer: type " << to_string(type)
                << " msg: " << p_callback_data->pMessage << std::endl;
    }

    return vk::False;
  }

  static std::vector<char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
  }
};

int main() {
  try {
    TriangleApplication app;
    app.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
