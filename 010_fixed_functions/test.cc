#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <optional>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include "config.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// 我们要启用的校验层列表
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// 设备扩展（创建逻辑设备时使用）
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// 通过宏控制是否启用校验层
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// 创建 VkDebugUtilsMessengerEXT 对象, 该对象来存储回调函数信息, 它将用于提交给 Vulkan 完成回调函数的设置.
// 由于 vkCreateDebugUtilsMessengerEXT 函数是一个扩展函数, 不会被 Vulkan 库自动加载, 所以需要我们自己使用 vkGetInstanceProcAddr 函数来加载它。
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) 
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;  // 绘制指令的队列族
    std::optional<uint32_t> presentFamily;   // 呈现的队列族

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;   // 基础表面特性 (交换链的最小/最大图像数量,最小/最大图像宽度、高度)
    std::vector<VkSurfaceFormatKHR> formats; // 表面格式 (像素格式,颜色空间)
    std::vector<VkPresentModeKHR> presentModes; // 可用的呈现模式
};

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;  // 该对象来存储回调函数信息
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;  // 逻辑设备来作为和物理设备交互的接口

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;  // 交换链
    std::vector<VkImage> swapChainImages; // 交换链图像句柄
    VkFormat swapChainImageFormat; // 表面格式
    VkExtent2D swapChainExtent;    // 交换范围 (分辨率)
    std::vector<VkImageView> swapChainImageViews;  // 图像视图

    VkPipelineLayout pipelineLayout;

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);  //需要在 Vulkan 实例被清除之前完成。
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // 创建实例
        // 填写应用程序信息,这些信息的填写不是必须的,但填写的信息可能会作为驱动程序的优化依据,让驱动程序进行一些特殊的优化。
        // 比如,应用程序使用了某个引擎,驱动程序对这个引擎有一些特殊处理,这时就可能有很大的优化提升.
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        createInfo.enabledLayerCount = 0;

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;   // 这里是为啥也要加一次 ???
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    // 逻辑设备的创建过程类似于我们之前描述的 Vulkan 实例的创建过程。我们还需要指定使用的队列所属的队列族。
    // 创建逻辑设备时指定的队列会随着逻辑设备一同被创建, 逻辑设备的队列会在逻辑设备清除时,自动被清除,所以不需要我们在 cleanup 函数中进行队列的清除操作。
    // 对于同一个物理设备,我们可以根据需求的不同,创建多个逻辑设备。
    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()}; //使用set对索引号去重
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        float queuePriority = 1.0f; //// Vulkan 需要我们赋予队列一个 0.0 到 1.0 之间的浮点数作为优先级来控制指令缓冲的执行顺序。即使只有一个队列,我们也要显式地赋予队列优先级
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;   //队列族索引号
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();  //队列信息
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures; //设备feature
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); //设备扩展
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // 我们可以对设备和 Vulkan 实例使用相同的校验层,不需要额外的扩展支持
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // 获取指定队列族的队列句柄。
        // 它的参数依次是逻辑设备对象,队列族索引,队列索引,用来存储返回的队列句柄的内存地址。
        // 因为,我们每个队列族只创建了一个队列,所以,可以直接使用索引 0 调用函数:
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats); // 表面格式
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);  // 呈现模式
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);  // 分辨率

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // 使用"最小图像个数 +1" 数量的图像来实现三倍缓冲 (猜测：最小图像个数 >= 2)
        // imageCount不要超过最大值
        // (注：maxImageCount 的值为 0 表明,只要内存可以满足,我们可以使用任意数量的图像。)
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount; // 注意： Vulkan 的具体实现可能会创建比这个最小交换链图像数量更多的交换链图像。
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;  // 每个图像所包含的层次, 通常是1。VR相关的应用会使用更多的层次。
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 在图像上进行怎样的操作, 这里我们指定为绘制操作，也就是将图像作为一个颜色附着来使用。

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        // 我们可以为交换链中的图像指定一个固定的变换操作 (需要交换链具有supportedTransforms 特性),比如顺时针旋转 90 度或是水平翻转。
        //    如果读者不需要进行任何变换操作,指定使用 currentTransform 变换即可。
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // 指定 alpha 通道是否被用来和窗口系统中的其它窗口进行混合操作。通常,我们将其设置为 VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR 来忽略掉 alpha 通道。
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        // clipped 成员变量被设置为VK_TRUE 表示我们不关心被窗口系统中的其它窗口遮挡的像素的颜色,这允许 Vulkan 采取一定的优化措施,但如果我们回读窗口的像素值就可能出现问题。
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;  // 用于交换链的重建

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // 获取交换链图像句柄
        //   我们需要显式地查询交换链图像数量,确保不会出错。
        //   交换链图像由交换链自己负责创建,并在交换链清除时自动被清除,不需要我们自己进行创建和清除操作。
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        // 记下我们设置的交换链图像格式和分辨率
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    // 创建图像视图
    // 使用任何 VkImage 对象,包括处于交换链中的,处于渲染管线中的,都需要我们创建一个 VkImageView 对象来绑定访问它。
    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  // components成员用于进行图像颜色通道的映射
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // subresourceRange 成员变量用于指定图像的用途和图像的哪一部分可以被访问
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    // 创建图形管线
    void createGraphicsPipeline() {
        auto vertShaderCode = readFile(TEST_BIN_PATH "/shader.vert.spv");
        auto fragShaderCode = readFile(TEST_BIN_PATH "/shader.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  // 指定着色器在管线的哪一阶段被使用
        vertShaderStageInfo.module = vertShaderModule; // 指定使用的着色器模块对象
        vertShaderStageInfo.pName = "main";  // 指定调用的着色器函数 (在同一份代码中可以实现多个片段着色器,然后通过不同的 "函数名" 调用它们)

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // 顶点输入
        // 由于我们直接在顶点着色器中硬编码顶点数据,所以我们填写结构体信息时指定不载入任何顶点数据。
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        // 输入装配
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 每三个顶点构成一个三角形图元
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // 视口和裁剪
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        // 需要注意,交换链图像的大小可能与窗口大小不同。交换链图像在之后会被用作帧缓冲,所以这里我们设置视口大小为交换链图像的大小。
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;  // 指定帧缓冲使用的深度值的范围
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent; // 在本教程,我们在整个帧缓冲上进行绘制操作,所以将裁剪范围设置为和帧缓冲大小一样

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // 光栅化
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE; // VK_TRUE 表示在近平面和远平面外的片段会被截断为在近平面和远平面上,而不是直接丢弃这些片段。
                                                // 这对于阴影贴图的生成很有用。使用这一设置需要开启相应的 GPU 特性。
        rasterizer.rasterizerDiscardEnable = VK_FALSE;  // VK_TRUE 表示所有几何图元都不能通过光栅化阶段。这一设置会禁止一切片段输出到帧缓冲。
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  // VK_POLYGON_MODE_FILL表示整个多边形,包括多边形内部都产生片段
        rasterizer.lineWidth = 1.0f;  // lineWidth 成员变量用于指定光栅化后的线段宽度,它以线宽所占的片段数目为单位。
                                      // 线宽的最大值依赖于硬件,使用大于 1.0f 的线宽,需要启用相应的 GPU 特性。
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // 指定使用的表面剔除类型
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // 指定顺时针的顶点序是正面,还是逆时针的顶点序是正面
        rasterizer.depthBiasEnable = VK_FALSE;  // 光栅化程序可以添加一个常量值或是一个基于片段所处线段的斜率得到的变量值到深度值上。
                                                // 这对于阴影贴图会很有用,但在这里,我们不使用它,所以将 depthBiasEnable 成员变量设置为 VK_FALSE。

        // 多重采样
        // 多重采样是一种组合多个不同多边形产生的片段的颜色来决定最终的像素颜色的技术,它可以一定程度上减少多边形边缘的走样现象。
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 颜色混合
        // 片段着色器返回的片段颜色需要和原来帧缓冲中对应像素的颜色进行混合。混合的方式有下面两种:
        //    1) 混合旧值和新值产生最终的颜色
        //    2) 使用位运算组合旧值和新值
        // 通常,我们使用颜色混合是为了进行 alpha 混合来实现半透明效果。
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // VK_FALSE,就不会进行混合操作。否则,就会执行指定的混合操作计算新的颜色值。计算出的新的颜色
                                                     // 值会按照 colorWriteMask 的设置决定写入到帧缓冲的颜色通道。

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        // 创建 VkPipelineLayout 对象
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // 注意，着色器模块对象只在管线创建时需要，用完了可以销毁
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    // 创建着色器模块对象
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        // 调用vkCreateShaderModule 函数后,code不再需要了，我们就可以立即释放掉存储着色器字节码的数组code内存

        return shaderModule;
    }

    // 选择最佳表面格式
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    // 选择最佳呈现模式
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // 选择最佳交换范围 ( 交换范围是交换链中图像的分辨率,它几乎总是和我们要显示图像的窗口的分辨率相同。 )
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        // Vulkan 通过 currentExtent 成员变量来告知适合我们窗口的交换范围。
        // 一些窗口系统会使用一个特殊值,uint32_t 变量类型的最大值,表示允许我们自己选择对于窗口最合适的交换范围,但我们选择的交换范围需要在 
        //    minImageExtent 与 maxImageExtent 的范围内。
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void createSurface() {
        // 我们可以使用 GLFW 库的 glfwCreateWin-dowSurface 函数来完成表面创建, 它在不同平台的实现是不同的, 可以跨平台使用。
        // GLFW并没有提供清除表面的函数，需要我们自己调用 vkDestroySurfaceKHR 接口.
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void pickPhysicalDevice() {
        // 获取所有的显卡设备
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // 选择最合适的显卡设备(得分最高的设备)
        std::multimap<int, VkPhysicalDevice> candidates;  //候选设备
        for (const auto& device : devices) {
            int score = rateDeviceSuitability(device);
            if (score > 0) {
                candidates.insert(std::make_pair(score, device));
            }
        }
        if (candidates.size() > 0) {
            physicalDevice = candidates.rbegin()->second;
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        // 打印选择的显卡设备名
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        std::cout << "pick device:" << deviceProperties.deviceName << std::endl;
    }

    int rateDeviceSuitability(VkPhysicalDevice device) {
        int score = 1;

        // 不符合要求的直接返回0
        if (!isDeviceSuitable(device)) {
            return 0;
        }

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        std::string dev_name(deviceProperties.deviceName);
        std::cout << "device name:" << dev_name << std::endl;
        if (dev_name.find("Intel") != std::string::npos) {
            score += 1;
        } else if (dev_name.find("NVIDIA") != std::string::npos) { // 这里我们优先NVIDIA设备
            score += 2;
        }

        return score;
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        // 检测是否有合适的队列族
        QueueFamilyIndices indices = findQueueFamilies(device);

        // 检测设备扩展是否支持
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // 检测交换链的能力是否满足需求
        // 需要至少支持一种图像格式和一种支持我们的窗口表面的呈现模式
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // 将所需的扩展保存在一个集合中,然后枚举所有可用的扩展,将集合中的扩展剔除,最后,如果这个集合中的元素为 0,说明我们所需的扩展全部都被满足。
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        // 获取设备的队列族属性
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // 检查队列族是否具有图形能力(绘制能力)
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // 检查物理设备的队列族是否具有呈现能力
            // ( 绘制指令队列族和呈现队列族可以是同一个队列族，也可能不是同一个 )
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        // 查询基础表面特性
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // 查询表面支持的格式
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // 查询支持的呈现模式
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    // 填充 VkDebugUtilsMessengerCreateInfoEXT
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        // createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        //                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        populateDebugMessengerCreateInfo(debugCreateInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    bool checkValidationLayerSupport() {
        // 获取所有可用的校验层
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // 检测要求的校验层是否可用
        for (const char* layerName : validationLayers) {
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

    // 获取实例扩展
    std::vector<const char*> getRequiredExtensions() {
        // 获取所有可用的实例扩展
        uint32_t availableExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());
        std::cout << "available extensions:\n";
        for (const auto& ext : availableExtensions) {
            std::cout << '\t' << ext.extensionName << '\n';
        }

        // Vulkan 是平台无关的 API,所以需要和窗口系统交互的扩展。 GLFW 库包含了一个可以返回这一扩展的函数
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        // 仅仅启用校验层并没有任何用处,我们不能得到任何有用的调试信息。为了获得调试信息,我们需要使用 VK_EXT_debug_utils 扩展,设置回调函数来接受调试信息。
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        std::cout << "required extensions:\n";
        for (std::size_t i = 0; i < extensions.size(); ++i) {
            std::cout << '\t' << extensions[i] << std::endl;
        }

        // 检测要求的扩展是否可用
        for (std::size_t i = 0; i < extensions.size(); ++i) {
            bool find = false;
            for (const auto& availableExt : availableExtensions) {
                if (strcmp(availableExt.extensionName, extensions[i]) == 0) {
                    find = true;
                    std::cout << "find extensions ok: " << extensions[i] << std::endl;
                    break;
                }
            }
            if (!find) {
                throw std::runtime_error("failed to find extensions");
            }
        }

        return extensions;
    }

    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    // debug输出的回调函数
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
            VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
    {
        std::cerr << "[DEBUG] " << pCallbackData->pMessage << std::endl;

        // 回调函数的返回值是有特定意义的: 
        //   该值用来表示引发校验层处理的 Vulkan API调用是否被中断。如果返回值为 true,对应 Vulkan API 调用就会返回VK_ERROR_VALIDATION_FAILED_EXT 错误代码。
        //   通常,只在测试校验层本身时会返回 true,其余情况下,回调函数应该返回 VK_FALSE。
        return VK_FALSE;
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}