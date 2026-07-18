#include "graphics.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>

#include <print>

namespace Graphics
{

    /// @brief Calculate the size of the given array at compile time (if possible)
    /// @tparam T The type of the array's element
    /// @tparam N Size of the array automatically detected
    /// @param arr 
    /// @return the unsigned int 32 size of the array
    template<typename T, std::size_t N>
    constexpr uint32_t VK_ARR_SIZE(T (&arr)[N])
    {
        return N;
    }

    /// Core graphics objects needed in every vulkan application.
    /// Their lifetime usually are equal to the entire application.
    struct
    {
        VkInstance Instance                         = {};
        VkDebugUtilsMessengerEXT DebugMessenger     = {};
        VkSurfaceKHR Surface                        = {};
        VkPhysicalDevice PhysDevice                 = {};
        VkDevice Device                             = {};
        VkQueue GraphicsQueue                       = {};
        VkQueue TransferQueue                       = {};
        uint32_t GraphcisQueueFamilyIndex           = {};
        uint32_t TransferQueueFamilyIndex           = {};
        VmaAllocator Allocator                      = {};
    } Context;

    struct
    {
        VkSwapchainKHR Swapchain                = {};
        VkExtent2D Extent                       = {};
        VkSurfaceFormatKHR SurfaceFormat        = {};
        VkImage* Images                         = {};
        VkImageView* ImageViews                 = {};
        VkSemaphore* RenderFinishedSemaphores   = {};

        VkImage MsaaColorImage                  = {};
        VkImageView MsaaColorImageView          = {};
        VmaAllocation MsaaColorMem              = {};

        VkImage DepthStencilImage               = {};
        VkImageView DepthStencilImageView       = {};
        VmaAllocation DepthStencilMem           = {};

        VkFramebuffer Framebuffers              = {};
    } Swapchain;

    
    /// @brief Max frame in flight
    constexpr size_t MAX_FIF = 2;

    /// @brief Hold per frame data
    struct
    {
        struct alignas(16)
        {
            float view[16]          = {};
            float projection[16]    = {};
        } Camera[MAX_FIF];

        VkBuffer UboBuffers[MAX_FIF]                    = {};
        VmaAllocation UboMems[MAX_FIF]                  = {};
        VkDescriptorSet DescriptorSets[MAX_FIF]         = {};
        VkSemaphore AcquiredImageSemaphores[MAX_FIF]    = {};
        VkFence SubmitFences[MAX_FIF]                   = {};
        VkCommandPool CmdPool[MAX_FIF]                  = {};
        VkCommandBuffer CmdBuffer[MAX_FIF]              = {};
    } Frame;

    /// @brief Pipeline object pack
    struct
    {
        VkRenderPass RenderPass                     = {};
        VkDescriptorSetLayout DescriptorSetLayout   = {};
        VkDescriptorPool DescriptorPool             = {};
        VkPipelineLayout PipelineLayout             = {};
        VkPipeline PbrPipeline                      = {};
    } Pipeline;

    /// @brief Shared storage buffer
    struct SSBOO
    {
        alignas(16) uint32_t TextureID  = {};
        alignas(16) glm::mat4 ModelMatrix = {};
    };

    struct
    {
        VkBuffer VertexBuffer           = {};
        VmaAllocation VertexAlloc       = {};

        VkDeviceSize VertexPosOffset    = {};
        VkDeviceSize VertexNormalOffset = {};
        VkDeviceSize VertexUvOffset     = {};

        VkBuffer IndexBuffer            = {};
        VmaAllocation IndexAlloc        = {};

        VkBuffer IndirectDrawBuffer     = {};
        VmaAllocation IndirectDrawAlloc = {};

        VkImage* TextureImages              = {};
        VkImageView* TextureImageViews      = {};
        VmaAllocation* TextureAllocs        = {};

        /// @brief One sampler used for every image
        VkSampler TextureSampler            = {};

        VkBuffer StageBuffer                = {};
        VmaAllocation StageAlloc            = {};
    } Scene;

    struct
    {
        bool ResizeRequested    = {};
        uint8_t FrameIndex     = {};
    } Loop;

    #pragma region Forward declaration | Functions
    void CreateInstance();
    void CreateSurface(SDL_Window* Window);
    void CreateDevice();
    void CreateSwapchain(VkSwapchainKHR PrevSwapchain);
    #pragma endregion

    void Initialize(SDL_Window* Window)
    {
        CreateInstance();
        CreateSurface(Window);
        CreateDevice();
        CreateSwapchain(VK_NULL_HANDLE);
    }

    void Update(double DeltaTime)
    {

    }

    void Shutdown()
    {
        vkDestroySwapchainKHR(Context.Device, Swapchain.Swapchain, nullptr);
        vmaDestroyAllocator(Context.Allocator);
        vkDeviceWaitIdle(Context.Device);
        vkDestroyDevice(Context.Device, nullptr);
        vkDestroySurfaceKHR(Context.Instance, Context.Surface, nullptr);
        vkDestroyInstance(Context.Instance, nullptr);
    }

    void CreateInstance()
    {
        // Init vulkan loader to setup the vulkan function pointers
        volkInitialize();

        // Vulkan instance layers requested
        const char* layers[] = {
            "VK_LAYER_KHRONOS_validation"   // @todo turn this off on release build
        };

        // Extensions requested by the application
        const char* extensions[] = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,          // @todo turn this off on release build
            VK_KHR_SURFACE_EXTENSION_NAME,              // surface khr
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,        // swapchain khr on windows
            "VK_KHR_get_physical_device_properties2"    // Vma needs this
        };

        // vulkan version 1.3+ to enable:
        // * VK_EXT_descriptor_indexing,
        // * VK_KHR_shader_draw_parameters,
        // * VK_KHR_dynamic_rendering
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName    = "Orda";
        appInfo.applicationVersion  = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion          = VK_MAKE_VERSION(1, 3, 0);

        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        // #ifdef _DEBUG
        // create_info.pNext = &DEBUG_UTILS_MESSENGER_CREATE_INFO;
        // #endif
        instanceCreateInfo.pApplicationInfo         = &appInfo;
        instanceCreateInfo.enabledLayerCount        = VK_ARR_SIZE(layers);
        instanceCreateInfo.ppEnabledLayerNames      = &layers[0];
        instanceCreateInfo.enabledExtensionCount    = VK_ARR_SIZE(extensions);
        instanceCreateInfo.ppEnabledExtensionNames  = &extensions[0];

        vkCreateInstance(&instanceCreateInfo, nullptr, &Context.Instance);  // @todo print the error type

        volkLoadInstance(Context.Instance);
    }

    void CreateSurface(SDL_Window* Window)
    {
        SDL_Vulkan_CreateSurface(Window, Context.Instance, &Context.Surface);
    }

    void CreateDevice()
    {
        // Gpu features required by the renderer
        VkPhysicalDeviceFeatures requiredFeatures = {};
        requiredFeatures.geometryShader        = VK_TRUE;
        requiredFeatures.tessellationShader    = VK_TRUE;
        requiredFeatures.multiDrawIndirect     = VK_TRUE;   // Enable Multi draw indirect
        requiredFeatures.fillModeNonSolid      = VK_TRUE;   // Enable Wireframe view
        requiredFeatures.sampleRateShading     = VK_TRUE;
        requiredFeatures.samplerAnisotropy     = VK_TRUE;   // Enable Multisampling

        const char* extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,    // Required by swapchain / presentation engine
            "VK_KHR_get_memory_requirements2",  // Required by Vma
            "VK_KHR_bind_memory2"               // Required by Vma
        };

        uint32_t physDeviceCount = 0;
        vkEnumeratePhysicalDevices(Context.Instance, &physDeviceCount, nullptr);

        VkPhysicalDevice physDevices[4] = {};  // We can list up to four physical devices.
        vkEnumeratePhysicalDevices(Context.Instance, &physDeviceCount, &physDevices[0]);

        for (uint32_t i = 0; i < physDeviceCount; i++)
        {
            VkPhysicalDeviceProperties physDeviceProp = {};
            vkGetPhysicalDeviceProperties(physDevices[i], &physDeviceProp);

            // Select the device based only on the device type
            if (physDeviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                Context.PhysDevice = physDevices[i];
                std::println("Vk: {}", physDeviceProp.deviceName);
                break;
            }
        }

        uint32_t queueFamilyCount = {};
        vkGetPhysicalDeviceQueueFamilyProperties(Context.PhysDevice, &queueFamilyCount, nullptr);

        VkQueueFamilyProperties queueFamilyProperties[8] = {};
        vkGetPhysicalDeviceQueueFamilyProperties(Context.PhysDevice, &queueFamilyCount, &queueFamilyProperties[0]);

        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            VkBool32 supportPresentation = {};
            vkGetPhysicalDeviceSurfaceSupportKHR(Context.PhysDevice, i, Context.Surface, &supportPresentation);

            if (Context.GraphcisQueueFamilyIndex == 0 && queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && supportPresentation)
            {
                Context.GraphcisQueueFamilyIndex = i;
                continue;
            }

            // if (Context.TransferQueueFamilyIndex == 0 && queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
            // {
            //     Context.TransferQueueFamilyIndex = i;
            // }
        }

        // Define queue priorities. Right now we have only one queue
        float priorities[] = {1.0f};
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex    = Context.GraphcisQueueFamilyIndex;
        queueCreateInfo.queueCount          = 1;
        queueCreateInfo.pQueuePriorities    = priorities;

        // Enable bindless descriptor (modern rendering technique)
        VkPhysicalDeviceVulkan12Features vk12Features = {};
        vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12Features.descriptorBindingPartiallyBound                = VK_TRUE;
        vk12Features.runtimeDescriptorArray                         = VK_TRUE;
        vk12Features.shaderSampledImageArrayNonUniformIndexing      = VK_TRUE;
        vk12Features.descriptorBindingSampledImageUpdateAfterBind   = VK_TRUE;
        vk12Features.descriptorBindingUpdateUnusedWhilePending      = VK_TRUE;

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.flags                      = 0;
        deviceCreateInfo.pNext                      = &vk12Features;
        deviceCreateInfo.queueCreateInfoCount       = 1;
        deviceCreateInfo.pQueueCreateInfos          = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount      = VK_ARR_SIZE(extensions);
        deviceCreateInfo.ppEnabledExtensionNames    = &extensions[0];
        deviceCreateInfo.pEnabledFeatures           = &requiredFeatures;

        vkCreateDevice(Context.PhysDevice, &deviceCreateInfo, nullptr, &Context.Device);
        volkLoadDevice(Context.Device);
        vkGetDeviceQueue(Context.Device, Context.GraphcisQueueFamilyIndex, 0, &Context.GraphicsQueue);

        VmaVulkanFunctions vmaFuncs = {};
        vmaFuncs.vkGetInstanceProcAddr                      = vkGetInstanceProcAddr;
        vmaFuncs.vkGetDeviceProcAddr                        = vkGetDeviceProcAddr;
        vmaFuncs.vkGetPhysicalDeviceProperties              = vkGetPhysicalDeviceProperties;
        vmaFuncs.vkGetPhysicalDeviceMemoryProperties        = vkGetPhysicalDeviceMemoryProperties;
        vmaFuncs.vkGetImageMemoryRequirements               = vkGetImageMemoryRequirements;
        vmaFuncs.vkGetBufferMemoryRequirements              = vkGetBufferMemoryRequirements;
        vmaFuncs.vkFlushMappedMemoryRanges                  = vkFlushMappedMemoryRanges;
        vmaFuncs.vkInvalidateMappedMemoryRanges             = vkInvalidateMappedMemoryRanges;
        vmaFuncs.vkAllocateMemory                           = vkAllocateMemory;
        vmaFuncs.vkFreeMemory                               = vkFreeMemory;
        vmaFuncs.vkMapMemory                                = vkMapMemory;
        vmaFuncs.vkUnmapMemory                              = vkUnmapMemory;
        vmaFuncs.vkCreateBuffer                             = vkCreateBuffer;
        vmaFuncs.vkDestroyBuffer                            = vkDestroyBuffer;
        vmaFuncs.vkCmdCopyBuffer                            = vkCmdCopyBuffer;
        vmaFuncs.vkCreateImage                              = vkCreateImage;
        vmaFuncs.vkDestroyImage                             = vkDestroyImage;
        vmaFuncs.vkBindBufferMemory                         = vkBindBufferMemory;
        vmaFuncs.vkBindImageMemory                          = vkBindImageMemory;
        vmaFuncs.vkBindBufferMemory2KHR                     = vkBindBufferMemory2KHR;
        vmaFuncs.vkBindImageMemory2KHR                      = vkBindImageMemory2KHR;
        vmaFuncs.vkGetBufferMemoryRequirements2KHR          = vkGetBufferMemoryRequirements2KHR;
        vmaFuncs.vkGetImageMemoryRequirements2KHR           = vkGetImageMemoryRequirements2KHR;
        vmaFuncs.vkGetPhysicalDeviceMemoryProperties2KHR    = vkGetPhysicalDeviceMemoryProperties2KHR;

        VmaAllocatorCreateInfo vmaAllocCreateInfo = {};
        vmaAllocCreateInfo.flags            = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        vmaAllocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        vmaAllocCreateInfo.physicalDevice   = Context.PhysDevice;
        vmaAllocCreateInfo.device           = Context.Device;
        vmaAllocCreateInfo.instance         = Context.Instance;
        vmaAllocCreateInfo.pVulkanFunctions = &vmaFuncs;

        vmaCreateAllocator(&vmaAllocCreateInfo, &Context.Allocator);
    }

    void CreateSwapchain(VkSwapchainKHR PrevSwapchain)
    {
        VkSurfaceCapabilitiesKHR caps = {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context.PhysDevice, Context.Surface, &caps);

        uint32_t surfaceFormatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(Context.PhysDevice, Context.Surface, &surfaceFormatCount, nullptr);

        VkSurfaceFormatKHR surfaceFormats[16]; // Assume there are no more than 16 surface formats listed
        vkGetPhysicalDeviceSurfaceFormatsKHR(Context.PhysDevice, Context.Surface, &surfaceFormatCount, &surfaceFormats[0]);

        VkSurfaceFormatKHR selectedSurfaceFormat = {};
        for (uint32_t i = 0; i < surfaceFormatCount; i++)
        {
            // Preferred surface format
            if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
                surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                selectedSurfaceFormat = surfaceFormats[i];
                break;
            }
        }

        // Fallback to the first surface format
        if (selectedSurfaceFormat.format == VK_FORMAT_UNDEFINED)
        {
            selectedSurfaceFormat = surfaceFormats[0];
        }

        constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        Swapchain.SurfaceFormat = selectedSurfaceFormat;
        Swapchain.Extent = caps.currentExtent;

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface          = Context.Surface;
        createInfo.minImageCount    = caps.minImageCount + 1;
        createInfo.imageFormat      = Swapchain.SurfaceFormat.format;
        createInfo.imageColorSpace  = Swapchain.SurfaceFormat.colorSpace;
        createInfo.imageExtent      = Swapchain.Extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform     = caps.currentTransform;
        createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode      = presentMode;
        createInfo.clipped          = VK_TRUE;
        createInfo.oldSwapchain     = PrevSwapchain;

        vkCreateSwapchainKHR(Context.Device, &createInfo, nullptr, &Swapchain.Swapchain);

        uint32_t swapchainImageCount = {};
        vkGetSwapchainImagesKHR(Context.Device, Swapchain.Swapchain, &swapchainImageCount, nullptr);
        // @todo make a double buffer to recycle memory allocations
        Swapchain.Images = static_cast<VkImage*>(_aligned_malloc(sizeof(VkImage) * swapchainImageCount, alignof(VkImage)));
        Swapchain.ImageViews = static_cast<VkImageView*>(_aligned_malloc(sizeof(VkImageView) * swapchainImageCount, alignof(VkImageView)));
        Swapchain.RenderFinishedSemaphores = static_cast<VkSemaphore*>(_aligned_malloc(sizeof(VkSemaphore) * swapchainImageCount, alignof(VkSemaphore)));
        vkGetSwapchainImagesKHR(Context.Device, Swapchain.Swapchain, &swapchainImageCount, &Swapchain.Images[0]);

        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel   = 0;
            subresourceRange.levelCount     = 1;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount     = 1;

            VkImageViewCreateInfo imageViewCreateInfo = {};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfo.pNext       = nullptr;
            imageViewCreateInfo.flags       = 0;
            imageViewCreateInfo.image       = Swapchain.Images[i];
            imageViewCreateInfo.viewType    = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfo.format      = Swapchain.SurfaceFormat.format;
            imageViewCreateInfo.components  = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            imageViewCreateInfo.subresourceRange = subresourceRange;

            vkCreateImageView(Context.Device, &imageViewCreateInfo, nullptr, &Swapchain.ImageViews[i]);
        }
    }
}