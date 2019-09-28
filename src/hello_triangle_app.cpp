#include "hello_triangle_app.h"
#include "scoped_glfw_window.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
    VkApplicationInfo create_vk_application_info() {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        return appInfo;
    }

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    ) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func == nullptr)
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator
    ) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func == nullptr)
            return;
        func(instance, debugMessenger, pAllocator);
    }

    std::string getVersionString(uint32_t version) {
        std::stringstream out;
        out << VK_VERSION_MAJOR(version) << '.'
            << VK_VERSION_MINOR(version) << '.'
            << VK_VERSION_PATCH(version);
        return out.str();
    }

    void print_extensions() {
        uint32_t extensionCount = 0u;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "available extensions:" << std::endl;
        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << std::endl;
        }
    }

    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }
}

namespace vulkan_tutorial {
    hello_triangle_app::hello_triangle_app()
        : _debugMessenger {nullptr},
          _device {VK_NULL_HANDLE},
          _deviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
          _graphicsQueue {VK_NULL_HANDLE},
          _instance {new VkInstance()},
          _physicalDevice {VK_NULL_HANDLE},
          _pipelineLayout {VK_NULL_HANDLE},
          _presentQueue {VK_NULL_HANDLE},
          _renderPass {VK_NULL_HANDLE},
          _surface {VK_NULL_HANDLE},
          _swapchain {VK_NULL_HANDLE},
          _swapchainExtent {0u, 0u},
          _swapchainFramebuffers {},
          _swapchainImageFormat {VK_FORMAT_UNDEFINED},
          _swapchainImages {},
          _swapchainImageViews {},
          _validationLayers {
            "VK_LAYER_KHRONOS_validation"
          }
    {}

    hello_triangle_app::~hello_triangle_app() {
        cleanup();
    }

    void hello_triangle_app::run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

    bool hello_triangle_app::checkDeviceExtensionsSupport(VkPhysicalDevice device) const {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(_deviceExtensions.begin(), _deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    bool hello_triangle_app::checkValidationLayerSupport() const {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::cout << "validation layers:" << std::endl;
        for (const auto& layerProperties : availableLayers) {
            std::cout << '\t' << layerProperties.layerName << std::endl;
        }

        for (const char* layerName : _validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    VkExtent2D hello_triangle_app::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        }

        VkExtent2D actualExtent = { INITIAL_WIDTH, INITIAL_HEIGHT };

        actualExtent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width)
        );
        actualExtent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height)
        );

        return actualExtent;
    }

    VkPresentModeKHR hello_triangle_app::chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes
    ) const {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                std::cout << "swap chain: selecting mailbox present mode" << std::endl;
                return availablePresentMode;
            }
        }

        std::cout << "swap chain: selecting FIFO present mode" << std::endl;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkSurfaceFormatKHR hello_triangle_app::chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats
    ) const {
        if (availableFormats.size() == 0) {
            throw std::runtime_error("no surface formats available for swap chain");
        }

        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8_UNORM
                && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    void hello_triangle_app::cleanup() {
        if (_instance == nullptr)
            return;

        for (auto framebuffer : _swapchainFramebuffers) {
            vkDestroyFramebuffer(_device, framebuffer, nullptr);
        }

        vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
        vkDestroyRenderPass(_device, _renderPass, nullptr);

        for (const auto& imageView: _swapchainImageViews) {
            vkDestroyImageView(_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkDestroySurfaceKHR(*_instance, _surface, nullptr);
        if (ENABLE_VALIDATION_LAYERS) {
            DestroyDebugUtilsMessengerEXT(*_instance, _debugMessenger, nullptr);
        }
        vkDestroyInstance(*_instance, nullptr);
        _window.destroy();
        glfwTerminate();

        _debugMessenger = VK_NULL_HANDLE;
        _device = VK_NULL_HANDLE;
        _instance.reset();
        _graphicsPipeline = VK_NULL_HANDLE;
        _graphicsQueue = VK_NULL_HANDLE;
        _physicalDevice = VK_NULL_HANDLE;
        _pipelineLayout = VK_NULL_HANDLE;
        _renderPass = VK_NULL_HANDLE;
        _surface = VK_NULL_HANDLE;
        _swapchain = VK_NULL_HANDLE;
        _swapchainFramebuffers.clear();
        _swapchainImageViews.clear();
        _window = scoped_glfw_window();
    }

    void hello_triangle_app::createFramebuffers() {
        _swapchainFramebuffers.resize(_swapchainImageViews.size());

        for (size_t i = 0; i < _swapchainImageViews.size(); ++i) {
            VkImageView attachments[] = {
                _swapchainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = _renderPass;
            framebufferInfo.attachmentCount = 1u;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = _swapchainExtent.width;
            framebufferInfo.height = _swapchainExtent.height;
            framebufferInfo.layers = 1u;

            VkResult result = vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]);
            if (result != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer");
        }
    }

    void hello_triangle_app::createGraphicsPipeline() {
        auto fragShaderCode = readFile("psmain.spv");
        std::cout << "read psmain.spv (" << fragShaderCode.size() << " bytes)" << std::endl;
        auto vertShaderCode = readFile("vsmain.spv");
        std::cout << "read vsmain.spv (" << vertShaderCode.size() << " bytes)" << std::endl;

        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0u;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0u;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) _swapchainExtent.width;
        viewport.height = (float) _swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = _swapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1u;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1u;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0u;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0u;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkResult result = vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline layout!");

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2u;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr;
        pipelineInfo.layout = _pipelineLayout;
        pipelineInfo.renderPass = _renderPass;
        pipelineInfo.subpass = 0u;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        result = vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create graphics pipeline");

        vkDestroyShaderModule(_device, vertShaderModule, nullptr);
        vkDestroyShaderModule(_device, fragShaderModule, nullptr);
    }

    void hello_triangle_app::createImageViews() {
        _swapchainImageViews.resize(_swapchainImages.size());

        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            std::cout << "create view for swap chain image #" << i << std::endl;
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = _swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = _swapchainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0u;
            createInfo.subresourceRange.levelCount = 1u;
            createInfo.subresourceRange.baseArrayLayer = 0u;
            createInfo.subresourceRange.layerCount = 1u;

            VkResult result = vkCreateImageView(_device, &createInfo, nullptr, &_swapchainImageViews[i]);
            if (result != VK_SUCCESS)
                throw std::runtime_error("failed to create swap chain image view");
        }
    }

    void hello_triangle_app::createInstance() {
        if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        auto appInfo = create_vk_application_info();
        auto extensions = getRequiredExtensions();

        print_extensions();

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (ENABLE_VALIDATION_LAYERS) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
            createInfo.ppEnabledLayerNames = _validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0u;
            createInfo.pNext = nullptr;
        }

        VkResult result = vkCreateInstance(&createInfo, nullptr, _instance.get());
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create VkInstance");
    }

    void hello_triangle_app::createLogicalDevice() {
        queue_family_indices indices = findQueueFamilies(_physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1u;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = _deviceExtensions.data();

        if (ENABLE_VALIDATION_LAYERS) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
            createInfo.ppEnabledLayerNames = _validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0u;
        }

        VkResult result = vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(_device, indices.graphicsFamily.value(), 0u, &_graphicsQueue);
        vkGetDeviceQueue(_device, indices.presentFamily.value(), 0u, &_presentQueue);
    }

    void hello_triangle_app::createRenderPass() {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = _swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0u;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1u;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1u;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1u;
        renderPassInfo.pSubpasses = &subpass;

        VkResult result = vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass");
    }

    VkShaderModule hello_triangle_app::createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VkResult result = vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module");

        return shaderModule;
    }

    void hello_triangle_app::createSurface() {
        VkResult result = glfwCreateWindowSurface(*_instance, _window.get(), nullptr, &_surface);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void hello_triangle_app::createSwapChain() {
        swap_chain_support_details swapchainSupport = querySwapChainSupport(_physicalDevice);

        VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);

        //uint32_t imageCount = swapchainSupport.capabilities.maxImageCount;
        uint32_t imageCount = swapchainSupport.capabilities.minImageCount;
        if (swapchainSupport.capabilities.maxImageCount > 0u
            && imageCount > swapchainSupport.capabilities.maxImageCount
        ) {
            imageCount = swapchainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = _surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1u;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        queue_family_indices indices = findQueueFamilies(_physicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2u;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0u;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = true;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapchain);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create the swap chain");

        vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, nullptr);
        _swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, _swapchainImages.data());

        _swapchainExtent = extent;
        _swapchainImageFormat = surfaceFormat.format;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL hello_triangle_app::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    queue_family_indices hello_triangle_app::findQueueFamilies(VkPhysicalDevice device) const {
        queue_family_indices indices;

        uint32_t queueFamilyCount = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (int i = 0; i < queueFamilies.size(); ++i) {
            const auto& queueFamily = queueFamilies[i];

            if (queueFamily.queueCount > 0
                && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }
        }

        return indices;
    }

    std::vector<const char*> hello_triangle_app::getRequiredExtensions() const {
        uint32_t glfwExtensionCount = 0u;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (ENABLE_VALIDATION_LAYERS) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void hello_triangle_app::initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
    }

    void hello_triangle_app::initWindow() {
        if (glfwInit() != GLFW_TRUE)
            throw std::runtime_error("glfwInit failed");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        _window = scoped_glfw_window();
        _window.init(INITIAL_WIDTH, INITIAL_HEIGHT, "Vulkan Test");
    }

    void hello_triangle_app::mainLoop() {
        while (glfwWindowShouldClose(_window.get()) == GLFW_FALSE) {
            glfwPollEvents();
        }
    }

    void hello_triangle_app::pickPhysicalDevice() {
        uint32_t deviceCount = 0u;
        vkEnumeratePhysicalDevices(*_instance, &deviceCount, nullptr);
        if (deviceCount == 0u) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(*_instance, &deviceCount, devices.data());

        std::multimap<int, VkPhysicalDevice> candidates;

        VkPhysicalDeviceProperties deviceProperties;
        for (int i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            std::cout << "device #" << i << ": " << deviceProperties.deviceName << std::endl;
            std::cout << "  api      : " << getVersionString(deviceProperties.apiVersion) << std::endl;
            std::cout << "  driver   : " << getVersionString(deviceProperties.driverVersion) << std::endl;
            std::cout << "  vendor_id: " << reinterpret_cast<void*>(deviceProperties.vendorID) << std::endl;
            std::cout << "  device_id: " << reinterpret_cast<void*>(deviceProperties.deviceID) << std::endl;

            int score = rateDeviceSuitability(device);
            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.rbegin()->first > 0) {
            _physicalDevice = candidates.rbegin()->second;
        }
        else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void hello_triangle_app::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = &debugCallback;
    }

    swap_chain_support_details hello_triangle_app::querySwapChainSupport(VkPhysicalDevice device) const {
        swap_chain_support_details details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
        if (formatCount > 0u) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
        if (presentModeCount > 0u) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    int hello_triangle_app::rateDeviceSuitability(VkPhysicalDevice device) const {
        if (!checkDeviceExtensionsSupport(device)) {
            return 0;
        }

        swap_chain_support_details swapChainSupport = querySwapChainSupport(device);
        if (swapChainSupport.formats.empty()
            || swapChainSupport.presentModes.empty())
        {
            return 0;
        }

        VkPhysicalDeviceFeatures deviceFeatures;
        VkPhysicalDeviceProperties deviceProperties;

        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        int score = 0;

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        score += deviceProperties.limits.maxImageDimension2D;

        queue_family_indices indices = findQueueFamilies(device);
        if (indices.isComplete())
            score += 1000;


        return score;
    }

    void hello_triangle_app::setupDebugMessenger() {
        if (!ENABLE_VALIDATION_LAYERS)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        populateDebugMessengerCreateInfo(createInfo);

        auto result = CreateDebugUtilsMessengerEXT(*_instance, &createInfo, nullptr, &_debugMessenger);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to set up the debug messenger!");
        }
    }
}
