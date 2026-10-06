#include "vulkan/GPUContext.h"

#include <set>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <utility>
#include <vector>

/**
 * Validation Layers that we want in our Vulkan Engine, A layer is code that sits between your program and the driver and inspects every Vulkan call.
 */
constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

/**
 * Extensions that our engine uses, in this case SwapChain Extension
 */
constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

GPUContext::GPUContext(SDL_Window* sdlWindow)
{
    const auto debugMessengerConfigInfo = GetDebugMessengerCreateInfo();
    CreateInstance(debugMessengerConfigInfo);
    CreateDebugMessenger(debugMessengerConfigInfo);
    CreateSurface(sdlWindow);
    PickPhysicalDevice();
    CreateLogicalDevice();
    std::cout << "Device created" << std::endl;
}

void GPUContext::CreateInstance(const VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo)
{
    /**  Structure specifying parameters of a newly created instance **/
    VkInstanceCreateInfo instanceCreateInfo{};

    /** sType is a VkStructureType value identifying this structure. **/
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    /** flags is a bitmask of VkInstanceCreateFlagBits indicating the behavior of the instance. **/
    instanceCreateInfo.flags = 0;

    const auto appInfo = GetAppInfo();

    /** ApplicationInfo is NULL or a pointer to a VkApplicationInfo structure.
     * If not NULL, this information helps implementations recognize behavior inherent to classes of applications.**/
    instanceCreateInfo.pApplicationInfo = &appInfo;

    /** Extensions **/
    uint32_t extensionCount;
    std::vector<const char*> extensions;
    /* ================== SDL Extension ======================= */
    const char* const* sdlInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!sdlInstanceExtensions)
    {
        throw std::runtime_error("failed to get required SDL extensions!");
    }

    for (int i = 0; std::cmp_less(i, extensionCount); ++i)
    {
        extensions.push_back(sdlInstanceExtensions[i]);
    }
    /* ================== Debug Extensions ======================= */

    if (kEnableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    /** ppEnabledExtensionNames is a pointer to an array of enabledExtensionCount null-terminated UTF-8 strings containing the names of extensions to enable. **/
    instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
    /** enabledExtensionCount is the number of global extensions to enable.**/
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());

    if (kEnableValidationLayers)
    {
        /** enabledLayerCount is the number of global layers to enable. **/
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        /** ppEnabledExtensionNames is a pointer to an array of enabledExtensionCount null-terminated UTF-8 strings containing the names of extensions to enable. **/
        instanceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();

        /** pNext is NULL or a pointer to a structure extending this structure. **/
        instanceCreateInfo.pNext = &debugCreateInfo;
    }
    else
    {
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }
}

void GPUContext::CreateDebugMessenger(const VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo)
{
    if constexpr (!kEnableValidationLayers) return;
    /** This function is an extension, so you ask for the address of it in runtime **/
    const auto createDebugFunction = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createDebugFunction != nullptr)
    {
        if (const VkResult result = createDebugFunction(instance, &debugCreateInfo, nullptr,
                                                        &debugMessenger); result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
    else
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void GPUContext::CreateSurface(SDL_Window* sdlWindow)
{
    if (!SDL_Vulkan_CreateSurface(sdlWindow, instance, nullptr, &surface))
    {
        throw std::runtime_error("failed to create window");
    }
}

void GPUContext::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::cout << "Found " << deviceCount << " devices" << std::endl;
    if (deviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (const auto& foundDevice : devices)
    {
        if (IsDeviceSuitable(foundDevice))
        {
            physicalDevice = foundDevice;
            queueFamilyIndices = FindQueueFamilies(physicalDevice);
            break;
        }
    }

    VkPhysicalDeviceProperties physicalDeviceProperties;

    if (physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    std::cout << "Using device: " << physicalDeviceProperties.deviceName << std::endl;
}

void GPUContext::CreateLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value()
    };
    constexpr float queuePriority = 1.0f;
    for (const uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        /* sType is a VkStructureType value identifying this structure. */
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        /* pNext is NULL or a pointer to a structure extending this structure. */
        /* flags is a bitmask indicating behavior of the queues. */
        /* queueFamilyIndex is an unsigned integer indicating the index of the queue family in which to create the queues on this device. 
         * This index corresponds to the index of an element of the pQueueFamilyProperties array that was returned by vkGetPhysicalDeviceQueueFamilyProperties. */
        queueCreateInfo.queueFamilyIndex = queueFamily;
        /* queueCount is an unsigned integer specifying the number of queues to create in the queue family 
         * indicated by queueFamilyIndex, and with the behavior specified by flags. */
        queueCreateInfo.queueCount = 1;
        /* pQueuePriorities is a pointer to an array of queueCount normalized floating-point values,
         * specifying priorities of work that will be submitted to each created queue. See Queue Priority for more information. */
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    /* 2 is the latest version */
    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkDeviceCreateInfo createInfo{};
    /* sType is a VkStructureType value identifying this structure. */
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    /* queueCreateInfoCount is the unsigned integer size of the pQueueCreateInfos array. */
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

    /* pQueueCreateInfos is a pointer to an array of VkDeviceQueueCreateInfo structures describing the
     * queues that are requested to be created along with the logical device.*/
    createInfo.pQueueCreateInfos = queueCreateInfos.data();


    /* pEnabledFeatures is NULL or a pointer to a VkPhysicalDeviceFeatures structure containing boolean indicators of 
     * all the features to be enabled. Refer to the Features section for further details.
     * This field is legacy. See Physical Device Queries: Superseded via version 2. */
    //createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.pNext = &deviceFeatures;

    /* enabledExtensionCount is the number of device extensions to enable. */
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());

    /* ppEnabledExtensionNames is a pointer to an array of enabledExtensionCount null-terminated UTF-8 strings
     * containing the names of extensions to enable for the created device */
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    /* ========== This section is deprecated by Vulkan but its in Scotts' code ======= */

    // if (kEnableValidationLayers) {
    //     /* enabledLayerCount is legacy and not used. Implementations must not access this parameter.
    //      * It must be 0. See Device Layers: Superseded via instance layers. */
    //     createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
    //     
    //     /* ppEnabledLayerNames is legacy and not used. Implementations must not access this parameter.
    //      * It must be NULL. See Device Layers: Superseded via instance layers. */
    //     createInfo.ppEnabledLayerNames = kValidationLayers.data();
    // }
    // else {
    //     createInfo.enabledLayerCount = 0;
    // }
    /* ========== END This section is deprecated by Vulkan but its in Scotts' code ======= */

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
}

GPUContext::~GPUContext()
{
    /** Destroy: logical device → surface → messenger → instance. */
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    const auto debugMessengerDestroyFunction = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (debugMessengerDestroyFunction != nullptr)
    {
        debugMessengerDestroyFunction(instance, debugMessenger, nullptr);
    }

    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

VkDebugUtilsMessengerCreateInfoEXT GPUContext::GetDebugMessengerCreateInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT debuggerCreateInfo{};
    /** sType is a VkStructureType value identifying this structure. */
    debuggerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    /** pNext is NULL or a pointer to a structure extending this structure. */
    // debuggerCreateInfo.pNext = ;

    /** flags is 0 and is reserved for future use. */
    // debuggerCreateInfo.flags  = NULL;

    /** messageSeverity is a bitmask of VkDebugUtilsMessageSeverityFlagBitsEXT specifying which severity of event(s) will cause this callback to be called. */
    debuggerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    /** messageType is a bitmask of VkDebugUtilsMessageTypeFlagBitsEXT specifying which type of event(s) will cause this callback to be called. */
    debuggerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    /** pfnUserCallback is the application callback function to call. */
    debuggerCreateInfo.pfnUserCallback = DebugMessengerCallback;

    /**  pUserData is NULL or an application-defined user data pointer to be passed to the callback. */
    // debuggerCreateInfo.pUserData  = NULL;


    return debuggerCreateInfo;
}

VkApplicationInfo GPUContext::GetAppInfo()
{
    VkApplicationInfo appInfo{};
    /** sType is a VkStructureType value identifying this structure.**/
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    /** pNext is NULL or a pointer to a structure extending this structure. */
    appInfo.pNext = nullptr;
    /** pApplicationName is NULL or is a pointer to a null-terminated UTF-8 string containing the name of the application. */
    appInfo.pApplicationName = "Sebastian's Spectacular SEntity SComponent System";
    /** applicationVersion is an unsigned integer variable containing the developer-supplied version number of the application. */
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    /** is NULL or is a pointer to a null-terminated UTF-8 string containing the name of the engine (if any) used to create the application. */
    appInfo.pEngineName = "Overly Documented Vulkan Engine";
    /** engineVersion is an unsigned integer variable containing the developer-supplied version number of the engine used to create the application. */
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    /** must be the highest version of Vulkan that the application is designed to use, encoded as described in Version Numbers.
     * The patch version number specified in apiVersion is ignored when creating an instance object.
     * The variant version of the instance must match that requested in apiVersion.*/
    appInfo.apiVersion = VK_API_VERSION_1_1;
    return appInfo;
}

bool GPUContext::IsDeviceSuitable(VkPhysicalDevice candidate) const
{
    /* ============ 1. Does it have the queue families I need? =========== */
    QueueFamilyIndices indices = FindQueueFamilies(candidate);
    if (!indices.IsComplete()) return false;

    /*  ============ 2. Does it support the swapchain extension? =========== */
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());

    bool supportsSwapchain = false;
    for (const auto& [extensionName, specVersion] : availableExtensions)
    {
        requiredExtensions.erase(extensionName);
        if (requiredExtensions.empty())
        {
            supportsSwapchain = true;
            break;
        }
    }
    if (!supportsSwapchain) return false;

    /*  ============ 3. Can the surface show anything from it? =========== */

    /*  ============ 4. Does it have the features I need?  ===========*/
    VkPhysicalDeviceFeatures2 supportedFeatures{};
    supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(candidate, &supportedFeatures);

    return supportedFeatures.features.samplerAnisotropy == VK_TRUE;
}

QueueFamilyIndices GPUContext::FindQueueFamilies(const VkPhysicalDevice physicalDevice_) const
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        /* Presenting is a question about the surface, so Vulkan answers it per family. */
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.IsComplete()) break;
    }
    return indices;
}


VkBool32 GPUContext::DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                            VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                            void* pUserData)
{
    std::string message = pCallbackData->pMessage;
    std::string header;
    std::string source;
    std::string severity;
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        severity = "[VERBOSE]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        severity = "[INFO]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        severity = "[WARNING]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        severity = "[ERROR]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
        severity = "[UNKNOWN]";
        break;
    }

    /* messageTypes is a bitmask, so one message can carry more than one type */
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) source += "[GENERAL]";
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) source += "[VALIDATION]";
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) source += "[PERFORMANCE]";
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) source += "[DEVICE ADDRESS BINDING]";
    if (source.empty()) source = "[UNKNOWN]";

    header = "Vulkan " + source + severity + " \n";
    constexpr size_t maxWidth = 80;
    size_t lineStart = 0;
    while (message.size() - lineStart > maxWidth)
    {
        /* Vulkan already broke this line, so the next line starts after its break */
        const size_t existingBreak = message.find('\n', lineStart);
        if (existingBreak != std::string::npos && existingBreak <= lineStart + maxWidth)
        {
            lineStart = existingBreak + 1;
            continue;
        }

        /* The last space that still fits on this line */
        size_t breakLocation = message.rfind(' ', lineStart + maxWidth);
        if (breakLocation == std::string::npos || breakLocation <= lineStart)
        {
            /* No space fits, so the word runs long and the line breaks after it */
            breakLocation = message.find_first_of(" \n", lineStart + maxWidth);
            if (breakLocation == std::string::npos) break;
        }

        if (message[breakLocation] == ' ') message[breakLocation] = '\n';
        lineStart = breakLocation + 1;
    }
    std::cerr << header << message << "\n\n";
    return VK_FALSE;
}
