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

    /// @brief Max images count for the swapchain
    /// Assume we never exced this number (usually 2 to 4 images are used).
    constexpr size_t MAX_SWAPCHAIN_IMAGES = 4;

    struct
    {
        /// @brief The actual swapchain images count used (retrieved at swapchain creation time).
        /// No heap allocation involved, the code remain simple for this project.
        uint32_t ImagesKhrCount = {};

        VkSwapchainKHR Swapchain                                    = {};
        VkExtent2D Extent                                           = {};
        VkSurfaceFormatKHR SurfaceFormat                            = {};
        VkImage Images[MAX_SWAPCHAIN_IMAGES]                        = {};
        VkImageView ImageViews[MAX_SWAPCHAIN_IMAGES]                = {};
        VkFramebuffer Framebuffers[MAX_SWAPCHAIN_IMAGES]            = {};
        VkSemaphore RenderFinishedSemaphores[MAX_SWAPCHAIN_IMAGES]  = {};

        VkImage MsaaColorImage          = {};
        VkImageView MsaaColorImageView  = {};
        VmaAllocation MsaaColorMem      = {};

        VkImage DepthStencilImage           = {};
        VkImageView DepthStencilImageView   = {};
        VmaAllocation DepthStencilMem       = {};

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

        VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface             = Context.Surface;
        swapchainCreateInfo.minImageCount       = caps.minImageCount + 1;
        swapchainCreateInfo.imageFormat         = Swapchain.SurfaceFormat.format;
        swapchainCreateInfo.imageColorSpace     = Swapchain.SurfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent         = Swapchain.Extent;
        swapchainCreateInfo.imageArrayLayers    = 1;
        swapchainCreateInfo.imageUsage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.imageSharingMode    = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.preTransform        = caps.currentTransform;
        swapchainCreateInfo.compositeAlpha      = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode         = presentMode;
        swapchainCreateInfo.clipped             = VK_TRUE;
        swapchainCreateInfo.oldSwapchain        = PrevSwapchain;

        vkCreateSwapchainKHR(Context.Device, &swapchainCreateInfo, nullptr, &Swapchain.Swapchain);

        // Swapchain uses a fixed size array. The code below might fail in case
        // the swapchain needs more images than 4.
        // We uses fixed size array to keep the code simple and low level.
        vkGetSwapchainImagesKHR(Context.Device, Swapchain.Swapchain, &Swapchain.ImagesKhrCount, nullptr);
        assert(&Swapchain.ImagesKhrCount <= MAX_SWAPCHAIN_IMAGES);
        vkGetSwapchainImagesKHR(Context.Device, Swapchain.Swapchain, &Swapchain.ImagesKhrCount, &Swapchain.Images[0]);

        // Create image views
        for (uint32_t i = 0; i < Swapchain.ImagesKhrCount; i++)
        {
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel   = 0;
            subresourceRange.levelCount     = 1;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount     = 1;

            VkImageViewCreateInfo imageViewCreateInfo = {};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfo.pNext               = nullptr;
            imageViewCreateInfo.flags               = 0;
            imageViewCreateInfo.image               = Swapchain.Images[i];
            imageViewCreateInfo.viewType            = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfo.format              = Swapchain.SurfaceFormat.format;
            imageViewCreateInfo.components          = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            imageViewCreateInfo.subresourceRange    = subresourceRange;

            vkCreateImageView(Context.Device, &imageViewCreateInfo, nullptr, &Swapchain.ImageViews[i]);
        }

        // Create msaa and depth-stencil
        constexpr VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_4_BIT;
        constexpr VkComponentMapping compSwizzle = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

        VkImageCreateInfo imageCreateInfos[2] = {};
        // Sample color
        imageCreateInfos[0].sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfos[0].flags           = 0;
        imageCreateInfos[0].imageType       = VK_IMAGE_TYPE_2D;
        imageCreateInfos[0].format          = Swapchain.SurfaceFormat.format;
        imageCreateInfos[0].extent          = { Swapchain.Extent.width, Swapchain.Extent.height, 1 };
        imageCreateInfos[0].mipLevels       = 1;
        imageCreateInfos[0].arrayLayers     = 1;
        imageCreateInfos[0].samples         = sampleCount;
        imageCreateInfos[0].tiling          = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfos[0].usage           = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageCreateInfos[0].sharingMode     = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfos[0].initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        // Depth + Stencil
        imageCreateInfos[1].sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfos[1].flags           = 0;
        imageCreateInfos[1].imageType       = VK_IMAGE_TYPE_2D;
        imageCreateInfos[1].format          = VK_FORMAT_D32_SFLOAT_S8_UINT;
        imageCreateInfos[1].extent          = { Swapchain.Extent.width, Swapchain.Extent.height, 1 };
        imageCreateInfos[1].mipLevels       = 1;
        imageCreateInfos[1].arrayLayers     = 1;
        imageCreateInfos[1].samples         = sampleCount;
        imageCreateInfos[1].tiling          = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfos[1].usage           = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageCreateInfos[1].sharingMode     = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfos[1].initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;


        VkImageSubresourceRange subresourceRanges[2] = {};
        // Sample Color
        subresourceRanges[0].aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRanges[0].baseMipLevel   = 0;
        subresourceRanges[0].levelCount     = 1;
        subresourceRanges[0].baseArrayLayer = 0;
        subresourceRanges[0].layerCount     = 1;
        // Depth + Stencil
        subresourceRanges[1].aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        subresourceRanges[1].baseMipLevel   = 0;
        subresourceRanges[1].levelCount     = 1;
        subresourceRanges[1].baseArrayLayer = 0;
        subresourceRanges[1].layerCount     = 1;

        VkImageViewCreateInfo imageViewCreateInfos[2] = {};
        // Sample Color
        imageViewCreateInfos[0].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfos[0].pNext               = nullptr;
        imageViewCreateInfos[0].flags               = 0;
        imageViewCreateInfos[0].image               = Swapchain.MsaaColorImage;
        imageViewCreateInfos[0].viewType            = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfos[0].format              = Swapchain.SurfaceFormat.format;
        imageViewCreateInfos[0].components          = compSwizzle;
        imageViewCreateInfos[0].subresourceRange    = subresourceRanges[0];
        // Depth + Stencil
        imageViewCreateInfos[1].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfos[1].pNext               = nullptr;
        imageViewCreateInfos[1].flags               = 0;
        imageViewCreateInfos[1].image               = Swapchain.DepthStencilImage;
        imageViewCreateInfos[1].viewType            = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfos[1].format              = VK_FORMAT_D32_SFLOAT_S8_UINT;
        imageViewCreateInfos[1].components          = compSwizzle;
        imageViewCreateInfos[1].subresourceRange    = subresourceRanges[1];

        VkImage* images[]           = {&Swapchain.MsaaColorImage, &Swapchain.DepthStencilImage};
        VkImageView* imageViews[]   = {&Swapchain.MsaaColorImageView, &Swapchain.DepthStencilImageView};
        VmaAllocation* allocs[]     = {&Swapchain.MsaaColorMem, &Swapchain.DepthStencilMem};

        for (uint32_t i = 0; i < Swapchain.ImagesKhrCount; i++)
        {
            vkCreateImage(Context.Device, &imageCreateInfos[i], nullptr, images[i]);

            VkMemoryRequirements memRequirements = {};
            vkGetImageMemoryRequirements(Context.Device, *images[i], &memRequirements);

            // @todo how do we allocate memory with vma????
            VmaAllocationCreateInfo allocInfo = {};

            vmaAllocateMemory(Context.Allocator, &memRequirements, )
        }


        // Create synchronization objects
        for (uint32_t i = 0; i < Swapchain.ImagesKhrCount; i++)
        {
            VkSemaphoreCreateInfo semCreateInfo = {};
            semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semCreateInfo.flags = 0;

            vkCreateSemaphore(Context.Device, &semCreateInfo, nullptr, &Swapchain.RenderFinishedSemaphores[i]);
        }
    }

    void RecreateSwapchain()
    {
        // You are trying to recreate the swapchain when none exist yet.
        assert(Context.Device == VK_NULL_HANDLE);
        assert(Swapchain.Swapchain == VK_NULL_HANDLE);

        vkDeviceWaitIdle(Context.Device);

        for (uint32_t i = 0; i < Swapchain.ImagesKhrCount; i++)
        {
            vkDestroyFramebuffer(Context.Device, Swapchain.Framebuffers[i], nullptr);
            vkDestroyImageView(Context.Device, Swapchain.ImageViews[i], nullptr);
            vkDestroySemaphore(Context.Device, Swapchain.RenderFinishedSemaphores[i], nullptr);
        }

        // Destroy msaa resources
        vkDestroyImageView(Context.Device, Swapchain.MsaaColorImageView, nullptr);
        vkDestroyImage(Context.Device, Swapchain.MsaaColorImage, nullptr);
        vmaFreeMemory(Context.Allocator, Swapchain.MsaaColorMem);

        // Destroy depth-stencil resources
        vkDestroyImageView(Context.Device, Swapchain.DepthStencilImageView, nullptr);
        vkDestroyImage(Context.Device, Swapchain.DepthStencilImage, nullptr);
        vmaFreeMemory(Context.Allocator, Swapchain.DepthStencilMem);

        VkSwapchainKHR oldSwapchain = Swapchain.Swapchain;
        CreateSwapchain(oldSwapchain);
        
        vkDestroySwapchainKHR(Context.Device, oldSwapchain, nullptr);
    }
}