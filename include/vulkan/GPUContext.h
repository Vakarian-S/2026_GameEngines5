#pragma once
#include <array>
#include <iostream>
#include <optional>
#include <ostream>
#include <vector>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#ifdef NDEBUG
/**
 * Boolean to enable validation layers such as the debug messenger and the Khronos validation layer.
 */
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

struct QueueFamilyIndices
{
    /* optional means that it contains no value until it is assigned.
     has_value() returns false if no value has ever been assigned. */
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool IsComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

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

/**
 * Vulkan GPU Context is represented by a combination of three core objects that separate the application runtime, 
 * the physical hardware, and the logical execution environment
 * [ VkInstance ] Global API connection & layer managements
 * [ VkPhysicalDevice ] Physical GPU selection & capability query
 * [ VkDevice ] The actual "Logical Context" where commands execute
 */
class GPUContext
{
public:
    GPUContext() = default;

    /**
     * Create: instance → messenger → surface → pick GPU → logical device.
     */
    void InitializeGPUContext(SDL_Window* sdlWindow);

    /**
     * Creates a VkInstance, the global API connection & layer management
     */
    void CreateInstance(const VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo);

    /**
     * Creates the Debug Messenger Extension
     */
    void CreateDebugMessenger(const VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo);

    /**
     * Creates a Vulkan surface with the given SDL window, following that the Vulkan Surface will be the entry point for the screen
     * @param SDLWindow The SDL window instance to use
     */
    void CreateSurface(SDL_Window* SDLWindow);

    /**
     * Picks the GPU device to be used
     */
    void PickPhysicalDevice();

    /**
     * Creates the VkDevice where the commands will execute.
     */
    void CreateLogicalDevice();


    /**
     * Destruction order has to be the reverse of creation
     */
    ~GPUContext();

    GPUContext(const GPUContext&) = delete;
    GPUContext& operator=(const GPUContext&) = delete;
    GPUContext(GPUContext&&) = delete;
    GPUContext& operator=(GPUContext&&) = delete;

private:
    /** The program's connection to the Vulkan library. Every other object here comes from it. */
    VkInstance instance = VK_NULL_HANDLE;

    /** The GPU this program chose. Vulkan owns it, so the destructor does not destroy it. */
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    /** The program's own session on the chosen GPU. Buffers, images and pipelines are created from it. */
    VkDevice device = VK_NULL_HANDLE;

    /** Sends the validation layer's messages to DebugMessengerCallback. Stays null in release builds. */
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    /** The window, as Vulkan sees it. The swapchain presents its images to this. */
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    /** Family Indices of the selected Physical Device **/
    QueueFamilyIndices queueFamilyIndices;

    /** Where draw commands are submitted. Vulkan destroys it together with the device. */
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    /** Where finished images are handed over to the window. Often the same queue as graphicsQueue. */
    VkQueue presentQueue = VK_NULL_HANDLE;

    /**
     * Application-defined debug messenger callback function
     * @param messageSeverity specifies the VkDebugUtilsMessageSeverityFlagBitsEXT that triggered this callback.
     * @param messageTypes  is a bitmask of VkDebugUtilsMessageTypeFlagBitsEXT specifying which type of event(s) triggered this callback.
     * @param pCallbackData contains all the callback related data in the VkDebugUtilsMessengerCallbackDataEXT structure.
     * @param pUserData  is the application-defined user data pointer, equal to the value of VkDebugUtilsMessengerCreateInfoEXT::pUserData specified when the VkDebugUtilsMessengerEXT object was created.
     * @return 
     */
    VKAPI_ATTR static VkBool32 VKAPI_CALL DebugMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    /**
     * Creates a debug messenger config struct
     * @return VkDebugUtilsMessengerCreateInfoEXT Config data for the debug messenger
     */
    static VkDebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo();

    /**
     * Creates a debug messenger config struct
     * @return VkApplicationInfo Structure specifying application information
     */
    static VkApplicationInfo GetAppInfo();

    /**
     * Answers whether one GPU can draw into the window.
     *
     * The GPU passes when it has a queue family that draws, a queue family that presents to the
     * surface, and the swapchain extension.
     *
     * @param candidate the GPU to check
     * @return true when the GPU passes every check
     */
    bool IsDeviceSuitable(VkPhysicalDevice candidate) const;

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice_) const;
};
