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

// debug

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

// structs

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily; // or without optional, just uint32_t
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
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
	VkDebugUtilsMessengerEXT debugMessenger;
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
	VkRenderPass renderPass;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr); // width, height, title, monitor to open the window on (optional), (only relevant to OpenGL)
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
		while (!glfwWindowShouldClose(window)) { // to keep the application running until either an error occurs or the window is closed
			glfwPollEvents();
			drawFrame(); // new for rendering an presentation
		}

		vkDeviceWaitIdle(device); // waits for operations in a specific command queue to be finished, to exit the program without errors
	}

	void cleanup() { // cleaning up ressources once the window is closed; newer methods are cleaned up first
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroyFence(device, inFlightFence, nullptr);
		vkDestroyCommandPool(device, commandPool, nullptr);

		for (auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		//vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		for (auto imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

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

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

		if (enableValidationLayers) { // validation layers
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		//uint32_t glfwExtensionCount = 0;
		//const char** glfwExtensions;

		//glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		//createInfo.enabledExtensionCount = glfwExtensionCount;
		//createInfo.ppEnabledExtensionNames = glfwExtensions;

		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance); // pointer to struct with creation info, pointer to custom allocator callbacks, pointer to the new project variable

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) { // check if the instance was created succesfully
			throw std::runtime_error("Failed to create instance!");
		}

		// chcecking for extension support (if needed, if yes delete the auto extensions = getRequiredExtensions, that's from debug)

		//uint32_t extensionCount = 0;
		//vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		//std::vector<VkExtensionProperties> extensions(extensionCount);

		//vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		//std::cout << "available extensions:\n"; // listing available extensions
		//for (const auto& extension : extensions) {
		//	std::cout << '\t' << extension.extensionName << '\n';
		//}

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

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("Failed to set up debug messenger.");
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void pickPhysicalDevice() {
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
		//VkDeviceQueueCreateInfo queueCreateInfo = {}; // struct
		//queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		//queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		//queueCreateInfo.queueCount = 1;

		// --- new (creating the presentation queue) ---
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;
		// --- old (creating the presentation queue) ---
		//queueCreateInfo.pQueuePriorities = &queuePriority;
		// --- new (creating the presentation queue) ---
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo = {};
		    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		    queueCreateInfo.queueFamilyIndex = queueFamily;
		    queueCreateInfo.queueCount = 1;
		    queueCreateInfo.pQueuePriorities = &queuePriority;
		    queueCreateInfos.push_back(queueCreateInfo);
		}

		// specifying used device features

		VkPhysicalDeviceFeatures deviceFeatures = {};

		// creating the logical device

		// --- old, new one is below (creating the presentation queue) ---
		//VkDeviceCreateInfo createInfo = {};
		//createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		//createInfo.pQueueCreateInfos = &queueCreateInfo;
		//createInfo.queueCreateInfoCount = 1;
		//createInfo.pEnabledFeatures = &deviceFeatures;

		// --- new (creating the presentation queue) ---
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;

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

		VkSwapchainCreateInfoKHR createInfo = {};
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
			VkImageViewCreateInfo createInfo = {};
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

		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		// creating shader modules

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

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

		// input assembly

		VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
		input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // because we're drawing a triangle (there's more, explained in graphics pipeline -> fixed functions -> input assembly
		input_assembly_info.primitiveRestartEnable = VK_FALSE; // if set to VK_TRUE: possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF

		// viewports and scissors (not yet needed here ig, it's in the recordCommandBuffer method)

		//VkViewport viewport = {}; // describes the region of the framebuffer that the output will be rendered to, almost always (0, 0) to (width, height)
		//viewport.x = 0.0f;
		//viewport.y = 0.0f;
		//viewport.width = (float)swapChainExtent.width;
		//viewport.height = (float)swapChainExtent.height;
		//viewport.minDepth = 0.0f; // minDepth may be higher than maxDepth
		//viewport.maxDepth = 1.0f;

		//VkRect2D scissor = {}; // scissor rectangles define in which regions pixels will actually be stored
		//scissor.offset = { 0, 0 };
		//scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewport_info = {}; // needs dynamic state enabled (like above)
		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.viewportCount = 1;
		//viewport_info.pViewports = &viewport; // if using multiple viewport and scissor rectangles: reference an array of them
		viewport_info.scissorCount = 1;
		//viewport_info.pScissors = &scissor;

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

		// dynamic state

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamic_state_info = {}; // will cause the configuration of these values to be ignored and you will be able (and required) to specify the data at drawing time
		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamic_state_info.pDynamicStates = dynamicStates.data();

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

		// conclusion: create graphics pipeline

		VkGraphicsPipelineCreateInfo pipeline_info = {};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = shaderStages;
		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly_info;
		pipeline_info.pViewportState = &viewport_info;
		pipeline_info.pRasterizationState = &rasterizer_info;
		pipeline_info.pMultisampleState = &multisample_info;
		pipeline_info.pDepthStencilState = nullptr;
		pipeline_info.pColorBlendState = &color_blend_state_info;
		pipeline_info.pDynamicState = &dynamic_state_info;
		pipeline_info.layout = pipelineLayout;
		pipeline_info.renderPass = renderPass;
		pipeline_info.subpass = 0; // index, there can be more than 1 subpass
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline
		pipeline_info.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphicsPipeline) != VK_SUCCESS) { // could create multiple VkPipeline objects in a single call
			throw std::runtime_error("Failed to create graphics pipeline.");
		}

		// destroying shader modules here

		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
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

	// render passes

	void createRenderPass() {
		// attachment description

		VkAttachmentDescription color_attachment_desc = {};
		color_attachment_desc.format = swapChainImageFormat; // should match the format of the swap chain images
		color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT; // use 1 sample unless you're doing multisampling
		// applies to color an depth data
		color_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // determines what to do with the data in the attachment before rendering and after rendering, clear the framebuffer to black before drawing a new frame
		color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // same, rendered contents will be stored in memory and can be read later
		// applies to stencil data
		color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // stencil is irrelevant when not doing anything with the stencil buffer
		color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // same
		color_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // undefined means we don't care, specifies which layout the image will have before the render pass begins
		color_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // for images to be presented in the swap chain, images need to be transitioned to specific layouts that are suitable for the operation that they're going to be involved in next

		// subpasses and attachment references

		VkAttachmentReference color_attachment_ref = {};
		color_attachment_ref.attachment = 0; // index of the attachment to reference (from the description array, which only consists of 1 element)
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // optimal for best performance

		VkSubpassDescription subpass_desc = {};
		subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_desc.colorAttachmentCount = 1;
		subpass_desc.pColorAttachments = &color_attachment_ref;

		// subpass dependencies (new in rendering an presentation)

		VkSubpassDependency subpass_dependency = {};
		subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // refers to the implicit subpass before or after the render pass
		subpass_dependency.dstSubpass = 0; // dstSubpass must always be higher than srcSubpass to prevent cycles in the dependency graph (unless one of the subpasses is VK_SUBPASS_EXTERNAL
		subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.srcAccessMask = 0;
		subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// render pass

		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment_desc;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass_desc;
		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &subpass_dependency;

		if (vkCreateRenderPass(device, &render_pass_info, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass.");
		}
	}

	// framebuffers

	void createFramebuffers() {
		swapChainFramebuffers.resize(swapChainImageViews.size()); // resize the container to hold all of the framebuffers

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebuffer_info = {};
			framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_info.renderPass = renderPass; // compatible render pass: framebuffer and render pass use the same number of attachments
			framebuffer_info.attachmentCount = 1;
			framebuffer_info.pAttachments = attachments;
			framebuffer_info.width = swapChainExtent.width;
			framebuffer_info.height = swapChainExtent.height;
			framebuffer_info.layers = 1; // number of layers in image arrays, here single images

			if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer.");
			}
		}
	}

	// command buffers

	// command pools: manage the memory that is used to store the buffers and command buffers are allocated from them

	void createCommandPool() {
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo command_pool_info = {};
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allows command buffers to be rerecorded individually, without this flag they all have to be reset together
		command_pool_info.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &command_pool_info, nullptr, &commandPool) != VK_SUCCESS) { // commands will be used throughout the program to draw things on the screen
			throw std::runtime_error("Failed to create command pool.");
		}
	}

	// command buffer allocation

	void createCommandBuffer() {
		VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
		command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		command_buffer_allocate_info.commandPool = commandPool;
		command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // PRIMARY: can be submitted to a queue for execution, but cannot be called from other command buffers; SECONDARY: can't be submitted directly, but can be called from primary command buffers
		command_buffer_allocate_info.commandBufferCount = 1; // we're only allocating one command buffer

		if (vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate command buffers.");
		}
	}

	// command buffer recording

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) { // writes the commands we want to execute into a command buffer
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0; // specifies how we're going to use the command buffer (3 types available)
		begin_info.pInheritanceInfo = nullptr; // only relevant for secondary command buffers, specifies which state to inherit from the calling primary command buffers

		if (vkBeginCommandBuffer(commandBuffer, &begin_info) != VK_SUCCESS) {
			throw std::runtime_error("Failed to begin recording command buffer.");
		}

		// starting a render pass

		VkRenderPassBeginInfo render_pass_begin_info = {};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.renderPass = renderPass;
		render_pass_begin_info.framebuffer = swapChainFramebuffers[imageIndex];
		render_pass_begin_info.renderArea.offset = { 0, 0 }; // size of the render area, where shader loads and stores will take place
		render_pass_begin_info.renderArea.extent = swapChainExtent;

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} }; // black with 100% opacity

		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE); // render pass can now begin; INLINE: render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed

		// basic drawing commands

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline); // GRAPHICS: the pipeline object is a graphics pipeline, not a compute one

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdDraw(commandBuffer, 3, 1, 0, 0); // draw command for the triangle
		// 1. vertexCount: even without having an vertex buffer, we have 3 vertices to draw
		// 2. instanceCount: for instance rendering, use 1 if you're not doing that
		// 3. firstVertex: offset into the vertex buffer, defines the lowest value of gl_VertexIndex
		// 4. firstInstance: offset for instanced rendering, defines the lowest value of gl_InstanceIndex

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) { // finished recording the command buffer
			throw std::runtime_error("Failed to record command buffer.");
		}
	}

	// rendering and presentation

	void drawFrame() {
		// synchronization

		// semaphores: used to add order between queue operations (work we submit to a queue), there's binary semaphore and timeline semaphore in Vulkan (here: binary); does not block host execution
		//			   we need 2 semaphores:
		//			   1. to signal that an image has been acquired from the swapchain and is ready for rendering
		//			   2. to signal that rendering has finished and presentation can happen
		// fences: similar, used to synchronize execution, but it is for ordering the execution on the CPU, there's signaled fences and unsignaled fences; does block host execution
		//			   we need 1 fence: to make sure only one frame is rendering at a time

		// waiting for the previous frame

		vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX); // waits on the host for either any or all of the fences to be signaled before returning
		vkResetFences(device, 1, &inFlightFence); // manually reset the fence to the unsignaled state

		// acquiring an image from the swap chain

		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex); // imageIndex refers to the VkImage in our swapChainImages array, we're going to use it to pick the VkFrameBuffer

		// recording the command buffer

		vkResetCommandBuffer(commandBuffer, 0);
		recordCommandBuffer(commandBuffer, imageIndex); // function we defined before

		// submitting the command buffer

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = waitSemaphores;
		submit_info.pWaitDstStageMask = waitStages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &commandBuffer;
		
		VkSemaphore signalSemaphore[] = { renderFinishedSemaphore };

		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signalSemaphore;

		if (vkQueueSubmit(graphicsQueue, 1, &submit_info, inFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("Failed to submit draw command buffer.");
		}

		// presentation

		VkPresentInfoKHR present_info = {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signalSemaphore;

		VkSwapchainKHR swapChains[] = { swapChain };

		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapChains;
		present_info.pImageIndices = &imageIndex;
		present_info.pResults = nullptr;

		vkQueuePresentKHR(presentQueue, &present_info); // submits the request to present an image to the swap chain
	}

	void createSyncObjects() {
		// synchronization

		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device, &semaphore_info, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphore_info, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
			vkCreateFence(device, &fence_info, nullptr, &inFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create semaphores or fence (synchronization).");
		}
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