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

// 由于 vkDestroyDebugUtilsMessengerEXT 函数是一个扩展函数, 不会被 Vulkan 库自动加载, 所以需要我们自己使用 vkGetInstanceProcAddr 函数来加载它。
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
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;  // 指令池对象用于管理指令缓冲对象使用的内存,并负责指令缓冲对象的分配。
    VkCommandBuffer commandBuffer; // 我们需要将所有要执行的操作记录在一个指令缓冲对象,然后提交给可以执行这些操作的队列

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

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
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        // 等待逻辑设备的操作结束执行才能退出
        //     因为drawFrame 函数中的操作是异步执行的。这意味着我们关闭应用程序窗口跳出主循环时,绘制操作和呈现操作可能仍在继续执行,
        //     这与我们紧接着进行的清除操作也是冲突的。
        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);

        // 使用Vulkan API创建的对象应该在 Vulkan实例 清除之前被清除。
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    // 创建实例
    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // 填写应用程序信息,这些信息的填写不是必须的,但填写的信息可能会作为驱动程序的优化依据,让驱动程序进行一些特殊的优化。
        //    比如,应用程序使用了某个引擎,驱动程序对这个引擎有一些特殊处理,这时就可能有很大的优化提升.
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

        // 指定全局扩展
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // 指定全局校验层
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            // populateDebugMessengerCreateInfo(debugCreateInfo);
            // createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;   // 这里是为啥也要加一次 ???
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

    // 创建渲染流程
    void createRenderPass() {
        VkAttachmentDescription colorAttachment{}; // 附着描述
        colorAttachment.format = swapChainImageFormat;   // 指定颜色缓冲附着的格式
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // 指定采样数,在这里,我们没有使用多重采样,所以将采样数设置为 1
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // 指定在渲染之前对附着中的数据进行的操作。(用于颜色和深度缓冲)
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 指定在渲染之后对附着中的数据进行的操作。(用于颜色和深度缓冲)
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // 指定在渲染之前对附着中的数据进行的操作。(用于模板缓冲)
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 指定在渲染之后对附着中的数据进行的操作。(用于模板缓冲)
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // 指定渲染流程开始前的图像布局方式
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // 指定渲染流程结束后的图像布局方式

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;  // 指定要引用的附着在附着描述结构体数组中的索引。 这里设置的颜色附着在数组中的索引会被片段着色器使用,
                                            //    对应我们在片段着色器中使用的 layout(location = 0) out vec4 outColor 语句。
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // 指定进行子流程时引用的附着使用的布局方式

        // 描述子流程
        //    一个渲染流程可以包含多个子流程。子流程依赖于上一流程处理后的帧缓冲内容。比如,许多叠加的后期处理效果就是在上一次的处理结果上进行的。
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
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

        // 创建管线对象
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;  // 指定之前创建的管线布局
        pipelineInfo.renderPass = renderPass;  // 引用之前创建的渲染流程对象
        pipelineInfo.subpass = 0;              // 子流程在子流程数组中的索引
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // 注意，着色器模块对象只在管线创建时需要，用完了可以销毁
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    // 创建帧缓冲
    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        // 为交换链的每一个图像视图对象创建对应的帧缓冲
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;  // width 和 height 成员变量用于指定帧缓冲的大小
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;  // 指定图像层数

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // 开始指令缓冲的记录操作 (注：调用 vkBeginCommandBuffer 函数会重置指令缓冲对象)
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        // 开始一个渲染流程 (注：所有可以记录指令到指令缓冲的函数的函数名都带有一个 vkCmd 前缀)
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            // 绑定图形管线
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            // 绘制
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        // 结束渲染流程
        vkCmdEndRenderPass(commandBuffer);

        // 结束指令缓冲的记录操作
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);  // CPU阻塞等待GPU结束执行上一次提交到的指令
        vkResetFences(device, 1, &inFlightFence);  // 将fence对象转到unsignaled状态。

        // 从交换链获取一张图像 (此处不会阻塞CPU，获取成功后会通知信号量imageAvailableSemaphore)
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex); // UINT64_MAX 表示禁用图像获取超时

        vkResetCommandBuffer(commandBuffer, /*VkCommandBufferResetFlagBits*/ 0);
        recordCommandBuffer(commandBuffer, imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // 指定队列开始执行前需要等待的信号量,以及需要等待的管线阶段。
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        // 指定实际被提交执行的指令缓冲对象。
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // 指定在指令缓冲执行结束后发出信号的信号量对象
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // vkQueueSubmit可以同时大批量提交数据
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; // 指定开始呈现操作需要等待的信号量

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains; // 指定用于呈现图像的交换链

        presentInfo.pImageIndices = &imageIndex; // 指定需要呈现的图像在交换链中的索引

        // 请求交换链进行图像呈现操作
        vkQueuePresentKHR(presentQueue, &presentInfo);
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

    // debug输出的回调函数 (参数 pUserData 是一个指向了我们设置回调函数时,传递的数据的指针)
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