// credits to https://vulkan-tutorial.com/ :)

#define GLFW_INCLUDE_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <GLFW/glfw3native.h>
#include <set>
#include <fstream>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// validate wheter the program is being compiled in debug mode or not

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG // or CHECK_VULKAN_RESULT
    const bool enableValidationLayers = false;
#else 
    const bool enableValidationLayers = true;
#endif

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
    VkDevice device;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkQueue presentQueue;
    VkPhysicalDevice physicalDevice;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    VkPipelineLayout pipelineLayout;

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr); // width, height, title, monitor to open the window on (optional), (only relevant to OpenGL)
    }

    void initVulkan() {
        createInstance();
        //setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) { // to keep the application running until either an error occurs or the window is closed
            glfwPollEvents();
        }
    }

    void cleanup() { // cleaning up ressources once the window is closed
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr); // destroying created instance, should be done right before the program exits

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {
        // check for validation layers first

        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }

        // creating an instance

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

        if (enableValidationLayers) { // validation layers
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        //createInfo.enabledLayerCount = 0;

        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance); // pointer to struct with creation info, pointer to custom allocator callbacks, pointer to the new project variable

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) { // check if the instance was created succesfully
            throw std::runtime_error("Failed to create instance!");
        }

        // chcecking for extension support (if needed)

        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> extensions(extensionCount);

        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "available extensions:\n"; // listing available extensions
        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }

        // create Vulkan window surface (if needed)
        
        //kSurfaceKHR surface;
        //VkResult err = glfwCreateWindowSurface(instance, window, NULL, &surface);
        //if (err) {
        //    std::cout << "Error";
        //}
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

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

    void pickPhysicalDevice() {
        physicalDevice = VK_NULL_HANDLE;

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) { // error if there are no supported devices
            throw std::runtime_error("No GPUs with Vulkan support found.");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) { // check if any physical devices meet the requirements
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) { // throw error if there's none
            throw std::runtime_error("No suitable GPU found.");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device) { // check if device is suitable
        //VkPhysicalDeviceProperties deviceProperties;
        //VkPhysicalDeviceFeatures deviceFeatures;
        //vkGetPhysicalDeviceProperties(device, &deviceProperties);
        //vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        //return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader;

        // OR(?)

        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // swap chain support
        bool swapChainAdequate = false;
        if (extensionsSupported) { // if these conditions are met, swap chain support is sufficient
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) { // check if all of the required extensions are there
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        
        return requiredExtensions.empty(); // should be empty (true)
    }

    // queue families

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily; // or without optional, just uint32_t
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) { // find at least one queue family that supports graphics
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) { // break if indices has got a value
                break;
            }
            i++;
        }

        return indices;
    }

    // logical device

    void createLogicalDevice() { // might be without parameter?

        // specifying the queues to be created

        //VkPhysicalDevice physicalDevice = physicalDevice;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // --- updated version in for loop below (creating the presentation queue) ---
        VkDeviceQueueCreateInfo queueCreateInfo{}; // struct
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;

        // --- new (creating the presentation queue) ---
        //std::vector<VkDeviceQueueCreateInfo> queueCreateInfo;
        //std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        float queuePriority = 1.0f;
        // --- old (creating the presentation queue) ---
        queueCreateInfo.pQueuePriorities = &queuePriority;
        // --- new (creating the presentation queue) ---
        //for (uint32_t queueFamily : uniqueQueueFamilies) {
        //    VkDeviceQueueCreateInfo queueCreateInfo{};
        //    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        //    queueCreateInfo.queueFamilyIndex = queueFamily;
        //    queueCreateInfo.queueCount = 1;
        //    queueCreateInfo.pQueuePriorities = &queuePriority;
        //    //queueCreateInfos.push_back(queueCreateInfo); // not working...
        //}

        // specifying used device features

        VkPhysicalDeviceFeatures deviceFeatures{};

        // creating the logical device

        // --- old, new one is below (creating the presentation queue) ---
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;

        // --- new (creating the presentation queue) ---
        //VkDeviceCreateInfo createInfo{};
        //createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        //createInfo.pQueueCreateInfos = queueCreateInfos.data();
        //createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        //createInfo.pEnabledFeatures = &deviceFeatures;

        //createInfo.enabledExtensionCount = 0;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Could not create logical device.");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        // --- new (creating the presentation queue) ---
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void createSurface() {
        // window surface creation

        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = glfwGetWin32Window(window); // used to get the raw hwnd from the GLFW window object
        createInfo.hinstance = GetModuleHandle(nullptr); // returns the hinstance handle of the current process

        if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Window creation failed.");
        }
    }

    // swap chain support

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr); // querying the supported surface formats

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr); // querying the supported presentation modes

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    // swap chain settings: surface format

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    // swap chain settings: presentation mode, looks for the best mode that is available

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode; // best mode (?)
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR; // mode guaranteed to be available
    }

    // swap chain settings: swap extent

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
            return capabilities.currentExtent;
        }
        else {
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

    // creating the swap chain

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // recommended to request at least one more image than the minimum

        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // always 1 unless you are developing a stereoscopic 3D application
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // or VK_IMAGE_USAGE_TRANSFER_DST_BIT

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // EXCLUSIVE: An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family. This option offers the best performance.
            // CONCURRENT: Images can be used across multiple queue families without explicit ownership transfers.
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // optional
            createInfo.pQueueFamilyIndices = nullptr; // optional
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // to specify that you do not want any transformation, simply specify the current transformation
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // specifies if the alpha channel should be used for blending with other windows in the window system, almost always ignore the alpha channel like this
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // true to not care about the color of pixels that are obscured
        createInfo.oldSwapchain = VK_NULL_HANDLE; // assume that we'll only ever create one swap chain

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain.");
        }

        // retrieving the swap chain images

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    // image views (quite literally a view into an image)

    void createImageViews() { // creates a basic image view for every image in the swap chain so that we can use them as color targets later on
        swapChainImageViews.resize(swapChainImages.size()); // resize the list to fit all of the image views we'll be creating

        for (size_t i = 0; i < swapChainImages.size(); i++) { // iterates over all of the swap chain images
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // treat images as 1D textures, 2D textures (here), 3D textures and cube maps
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // swizzle the color channels around, can also map constant values of 0 and 1 to a channel, here: default mapping
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views.");
            }
        }
    }

    void createGraphicsPipeline() {

        // loading shader

        auto vertShaderCode = readFile("shader/vert.spv");
        auto fragShaderCode = readFile("shader/frag.spv");

        // creating shader modules

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);

        // shader stage creation

        VkPipelineShaderStageCreateInfo vert_shader_info = {};
        vert_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT; // pipeline stage the shader is going to be used in (enum values for this are described in introduction chapter)
        vert_shader_info.module = vertShaderModule; // shader module containing the code
        vert_shader_info.pName = "main"; // function to invoke, known as the entrypoint (it's possible to combine multiple fragment shaders into a single shader module and use different entry points to differentiate between their behaviors)
        vert_shader_info.pSpecializationInfo = nullptr; // specifies values for shader constants (if needed)

        VkPipelineShaderStageCreateInfo frag_shader_info = {};
        frag_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_info.module = fragShaderModule;
        frag_shader_info.pName = "main";
        frag_shader_info.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo shaderStages[] = { vert_shader_info, frag_shader_info };

        // vertex input

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {}; // describes the format of the vertex data that will be passed to the vertex shader
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = nullptr; // points to an array of structs that describe the aforementioned details for loading vertex data
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = nullptr; // points to an array of structs that describe the aforementioned details for loading vertex data

        // dynamic state

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_info = {}; // will cause the configuration of these values to be ignored and you will be able (and required) to specify the data at drawing time
        dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamic_state_info.pDynamicStates = dynamicStates.data();

        // input assembly

        VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // because we're drawing a triangle (there's more, explained in graphics pipeline -> fixed functions -> input assembly
        input_assembly_info.primitiveRestartEnable = VK_FALSE; // if set to VK_TRUE: possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF

        // viewports and scissors

        VkViewport viewport = {}; // describes the region of the framebuffer that the output will be rendered to, almost always (0, 0) to (width, height)
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f; // minDepth may be higher than maxDepth
        viewport.maxDepth = 1.0f; 

        VkRect2D scissor = {}; // scissor rectangles define in which regions pixels will actually be stored
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;

        VkPipelineViewportStateCreateInfo viewport_info = {}; // needs dynamic state enabled (like above)
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.viewportCount = 1;
        viewport_info.pViewports = &viewport; // if using multiple viewport and scissor rectangles: reference an array of them
        viewport_info.scissorCount = 1;
        viewport_info.pScissors = &scissor;

        // rasterizer

        VkPipelineRasterizationStateCreateInfo rasterizer_info = {};
        rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_info.depthClampEnable = VK_FALSE; // if set to VK_TRUE: fragments that are beyond the near and far planes are clamped to them as opposed to discarding them
        rasterizer_info.rasterizerDiscardEnable = VK_FALSE; // if set to VK_TRUE: geometry never passes through the rasterizer stage, disables any output to the framebuffer
        rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL; // determines how fragments are generated for geometry (how vertices are drawn); 3 modes: fill, line, point
        rasterizer_info.lineWidth = 1.0f; // for higher than 1.0 enable wideLines GPU feature
        rasterizer_info.cullMode - VK_CULL_MODE_BACK_BIT; // determines the type of face culling to use
        rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE; // specifies the vertex order for faces to be considered front-facing
        rasterizer_info.depthBiasEnable = VK_FALSE; // if set to true, this and the following values are used for shadow mapping
        rasterizer_info.depthBiasConstantFactor = 0.0f;
        rasterizer_info.depthBiasClamp = 0.0f;
        rasterizer_info.depthBiasSlopeFactor = 0.0f;

        // multisampling

        VkPipelineMultisampleStateCreateInfo multisample_info = {}; // one of the ways to perform anti-aliasing
        multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_info.sampleShadingEnable = VK_FALSE; // multisample is disabled for now
        multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_info.minSampleShading - 1.0f;
        multisample_info.pSampleMask = nullptr;
        multisample_info.alphaToCoverageEnable = VK_FALSE;
        multisample_info.alphaToOneEnable = VK_FALSE;

        // depth and stencil testing (not used rn)

        VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};

        // color blending

        VkPipelineColorBlendAttachmentState color_blend_attachment_info = {}; // contains the configuration per attached framebuffer 
        color_blend_attachment_info.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment_info.blendEnable = VK_FALSE;
        color_blend_attachment_info.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_info.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_info.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_info.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_info.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_info.alphaBlendOp = VK_BLEND_OP_ADD;
        // possibility for enabled blending: using alpha blending, the new color is blended with the old color based on its opacity
        //color_blend_attachment_info.blendEnable = VK_TRUE;
        //color_blend_attachment_info.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        //color_blend_attachment_info.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        //color_blend_attachment_info.colorBlendOp = VK_BLEND_OP_ADD;
        //color_blend_attachment_info.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        //color_blend_attachment_info.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        //color_blend_attachment_info.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state_info = {}; // contains the global color blending setting
        color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_info.logicOpEnable = VK_FALSE; // if set to VK_TRUE: using method of blending called bitwise combination
        color_blend_state_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_info.attachmentCount = 1;
        color_blend_state_info.pAttachments = &color_blend_attachment_info;
        color_blend_state_info.blendConstants[0] = 0.0f;
        color_blend_state_info.blendConstants[1] = 0.0f;
        color_blend_state_info.blendConstants[2] = 0.0f;
        color_blend_state_info.blendConstants[3] = 0.0f;

        // pipeline layout

        VkPipelineLayoutCreateInfo pipeline_layout_info = {}; // creating an empty pipeline layout for now
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pSetLayouts = nullptr;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout.");
        }
    }

    static std::vector<char> readFile(const std::string& filename) { // reads all of the bytes from the specified file and return them in a byte array managed by std::vector
        std::ifstream file(filename, std::ios::ate | std::ios::binary); // ate: start reading at the end of the file, binary: read the file as a binary file (avoid text transformation)

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file.");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo shader_crate_info = {};
        shader_crate_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_crate_info.codeSize = code.size();
        shader_crate_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &shader_crate_info, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module.");
        }

        return shaderModule;
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}