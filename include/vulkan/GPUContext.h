#pragma once
#include <array>
#include <iostream>
#include <optional>
#include <ostream>
#include <vector>
#include <vulkan/vulkan.h>

struct SDL_Window;

#ifdef NDEBUG
/**
 * Boolean to enable validation layers such as the debug messenger and the Khronos validation layer.
 */
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

/**
 * Which queue families this program uses on a GPU.
 *
 * A GPU sorts its queues into families, and every queue in a family can do the same kinds of
 * work. Vulkan lists the families, and a family is named by its position in that list.
 */
struct QueueFamilyIndices
{
    /** Position of a family that can run draw commands. Empty until FindQueueFamilies finds one. */
    std::optional<uint32_t> graphicsFamily;

    /** Position of a family that can present images to the surface. Often the same as graphicsFamily. */
    std::optional<uint32_t> presentFamily;

    /** @return true when both families were found */
    bool IsComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * A struct containing the details about the supported capabilities of a given Swap Chain
 */
struct SwapChainSupportDetails
{
    /**
     * The limits. Minimum and maximum image count, and the allowed image sizes.
     */
    VkSurfaceCapabilitiesKHR capabilities;
    /**
     * The pixel formats and colour spaces the window can show.
     */
    std::vector<VkSurfaceFormatKHR> formats;
    /**
     * The ways an image can reach the screen. FIFO waits for the screen's refresh. Mailbox and immediate do not wait.
     */
    std::vector<VkPresentModeKHR> presentModes;
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
    /**
     * Create: instance → messenger → surface → pick GPU → logical device.
     */
    explicit GPUContext(SDL_Window* sdlWindow);


    /**
     * Destruction order has to be the reverse of creation
     */
    ~GPUContext();

    GPUContext(const GPUContext&) = delete;
    GPUContext& operator=(const GPUContext&) = delete;
    GPUContext(GPUContext&&) = delete;
    GPUContext& operator=(GPUContext&&) = delete;

public:
    /** @return the program's session on the chosen GPU. Almost every Vulkan call takes it */
    VkDevice GetDevice() const { return device; }

    /** @return the chosen GPU itself, used to ask what the GPU supports */
    VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice; }

    /** @return the window, as Vulkan sees it */
    VkSurfaceKHR GetSurface() const { return surface; }

    /** @return the queue that draw commands and copy commands are submitted to */
    VkQueue GetGraphicsQueue() const { return graphicsQueue; }

    /** @return the queue that hands finished images to the window */
    VkQueue GetPresentQueue() const { return presentQueue; }

    /** @return the positions of the graphics and present queue families on the chosen GPU */
    QueueFamilyIndices GetQueueFamilyIndices() const { return queueFamilyIndices; }


    /**
     * Queries the supported Swap Chain features from the given physical device
     * @param physicalDevice_ device to query
     * @return A struct containing the information of the supported features of the given swapchain.
     */
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice_) const;

    /**
     * Picks which kind of GPU memory a buffer or image gets. A GPU offers a short list of memory types.
     * @param typeFilter the memory types the buffer or image allows, one bit per type
     * @param properties what the memory must offer, such as host visible or device local
     * @return the position of the first memory type that passes both tests. Throws when none does
     */
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

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

    /** The queue families of the chosen GPU. CreateLogicalDevice opens one queue in each. */
    QueueFamilyIndices queueFamilyIndices;

    /** Where draw commands are submitted. Vulkan destroys it together with the device. */
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    /** Where finished images are handed over to the window. Often the same queue as graphicsQueue. */
    VkQueue presentQueue = VK_NULL_HANDLE;

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
     * @param sdlWindow The SDL window instance to use
     */
    void CreateSurface(SDL_Window* sdlWindow);

    /**
     * Picks the GPU device to be used
     */
    void PickPhysicalDevice();

    /**
     * Creates the VkDevice where the commands will execute.
     */
    void CreateLogicalDevice();

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

    /**
     * Finds the Queue families available on the given GPU
     * @param physicalDevice_ GPU to check for
     * @return a struct containing the found information of the queue families.
     */
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice_) const;
};
