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

    /// @brief Calculate the total ^2 size
    /// @param size The actual size of the type
    /// @param alignemnt The actual alignment of the type
    /// @return The required size aligned to ^2
    constexpr VkDeviceSize VK_ALIGN(size_t size, size_t alignemnt)
    {
        return (size + alignemnt - 1) & ~(alignemnt - 1);
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
        VmaAllocationInfo StageAllocInfo    = {};
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
    void CreateRendererPass();
    void CreateFramebuffers();
    void RecreateSwapchain();
    void CreateFrameInFlight();
    void CreateStageBuffer();
    void PrepareSwapchainImages();
    void CreatePipeline();

    // @todo
    // Create batch
    // Create ubo
    // Create pipeline
    // Load textures
    #pragma endregion

    void Initialize(SDL_Window* Window)
    {
        CreateInstance();
        CreateSurface(Window);
        CreateDevice();
        CreateSwapchain(VK_NULL_HANDLE);
        CreateRendererPass();
        CreateFramebuffers();
        CreateFrameInFlight();
        CreateStageBuffer();

        PrepareSwapchainImages();
    }

    void Update(double DeltaTime)
    {
        if (Loop.ResizeRequested)
        {
            RecreateSwapchain();
            Loop.ResizeRequested = false;
        }

        vkWaitForFences(Context.Device, 1, &Frame.SubmitFences[Loop.FrameIndex], VK_TRUE, UINT64_MAX);
        uint32_t nextImage = 0;
        const VkResult acquireImageResult = vkAcquireNextImageKHR(Context.Device, Swapchain.Swapchain, UINT64_MAX, Frame.AcquiredImageSemaphores[Loop.FrameIndex], VK_NULL_HANDLE, &nextImage);

        if (acquireImageResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Loop.ResizeRequested = true;
            return;
        }

        vkResetFences(Context.Device, 1, &Frame.SubmitFences[Loop.FrameIndex]);
        vkResetCommandPool(Context.Device, Frame.CmdPool[Loop.FrameIndex], 0);

        VkCommandBufferBeginInfo cmdBeginInfo = {};
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.flags              = 0;
        cmdBeginInfo.pInheritanceInfo   = nullptr;
        vkBeginCommandBuffer(Frame.CmdBuffer[Loop.FrameIndex], &cmdBeginInfo);

        VkClearValue clearValues[2] = {};
        clearValues[0].color           = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f }};
        clearValues[1].depthStencil    = {1.0f, 0};

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = Pipeline.RenderPass;
        renderPassBeginInfo.framebuffer = Swapchain.Framebuffers[nextImage];
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = Swapchain.Extent;
        renderPassBeginInfo.clearValueCount = VK_ARR_SIZE(clearValues);
        renderPassBeginInfo.pClearValues = &clearValues[0];
        vkCmdBeginRenderPass(Frame.CmdBuffer[Loop.FrameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {};
        viewport.x          = 0.0f;
        viewport.y          = 0.0f;
        viewport.width      = static_cast<float>(Swapchain.Extent.width);
        viewport.height     = static_cast<float>(Swapchain.Extent.height);
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        vkCmdSetViewport(Frame.CmdBuffer[Loop.FrameIndex], 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = Swapchain.Extent;
        vkCmdSetScissor(Frame.CmdBuffer[Loop.FrameIndex], 0, 1, &scissor);

        vkCmdEndRenderPass(Frame.CmdBuffer[Loop.FrameIndex]);
        vkEndCommandBuffer(Frame.CmdBuffer[Loop.FrameIndex]);

        VkSemaphore waitSemaphores[1] = {};
        waitSemaphores[0] = Frame.AcquiredImageSemaphores[Loop.FrameIndex];

        VkPipelineStageFlags waitStages[1] = {};
        waitStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSemaphore signalSemaphores[1] = {};
        signalSemaphores[0] = Swapchain.RenderFinishedSemaphores[nextImage];

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = VK_ARR_SIZE(waitSemaphores);
        submitInfo.pWaitSemaphores      = &waitSemaphores[0];
        submitInfo.pWaitDstStageMask    = &waitStages[0];
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &Frame.CmdBuffer[Loop.FrameIndex];
        submitInfo.signalSemaphoreCount = VK_ARR_SIZE(signalSemaphores);
        submitInfo.pSignalSemaphores    = &signalSemaphores[0];
        vkQueueSubmit(Context.GraphicsQueue, 1, &submitInfo, Frame.SubmitFences[Loop.FrameIndex]);

        VkResult presentInfoResult = {};
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount  = 1;
        presentInfo.pWaitSemaphores     = &Swapchain.RenderFinishedSemaphores[nextImage];
        presentInfo.swapchainCount      = 1;
        presentInfo.pSwapchains         = &Swapchain.Swapchain;
        presentInfo.pImageIndices       = &nextImage;
        presentInfo.pResults            = &presentInfoResult;

        if (vkQueuePresentKHR(Context.GraphicsQueue, &presentInfo) == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Loop.ResizeRequested = true;
            return;
        }

        Loop.FrameIndex = (Loop.FrameIndex + 1) % MAX_FIF;
    }

    void Shutdown()
    {
        vkDestroyRenderPass(Context.Device, Pipeline.RenderPass, nullptr);

        for (uint32_t i = 0; i < Swapchain.ImagesKhrCount; i++)
        {
            vkDestroyFramebuffer(Context.Device, Swapchain.Framebuffers[i], nullptr);
            vkDestroyImageView(Context.Device, Swapchain.ImageViews[i], nullptr);
            vkDestroySemaphore(Context.Device, Swapchain.RenderFinishedSemaphores[i], nullptr);
        }

        // Destroy msaa resources
        vkDestroyImageView(Context.Device, Swapchain.MsaaColorImageView, nullptr);
        vmaDestroyImage(Context.Allocator, Swapchain.MsaaColorImage, Swapchain.MsaaColorMem);

        // Destroy depth-stencil resources
        vkDestroyImageView(Context.Device, Swapchain.DepthStencilImageView, nullptr);
        vmaDestroyImage(Context.Allocator, Swapchain.DepthStencilImage, Swapchain.DepthStencilMem);

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
        assert(Swapchain.ImagesKhrCount <= MAX_SWAPCHAIN_IMAGES);
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

        VkImage* images[]           = {&Swapchain.MsaaColorImage, &Swapchain.DepthStencilImage};
        VmaAllocation* allocs[]     = {&Swapchain.MsaaColorMem, &Swapchain.DepthStencilMem};

        for (uint32_t i = 0; i < Swapchain.ImagesKhrCount; i++)
        {
            VmaAllocationCreateInfo allocCreateInfo = {};
            allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            vmaCreateImage(Context.Allocator, &imageCreateInfos[i], &allocCreateInfo, images[i], allocs[i], nullptr);
        }

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

        VkImageView* imageViews[]   = {&Swapchain.MsaaColorImageView, &Swapchain.DepthStencilImageView};

        for (uint32_t i = 0; i < VK_ARR_SIZE(imageViews); i++)
        {
            vkCreateImageView(Context.Device, &imageViewCreateInfos[i], nullptr, imageViews[i]);
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

    void CreateRendererPass()
    {
        constexpr VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_4_BIT;

        VkAttachmentDescription colorAttachemnt = {};
        colorAttachemnt.flags           = 0;
        colorAttachemnt.format          = Swapchain.SurfaceFormat.format;
        colorAttachemnt.samples         = sampleCount;
        colorAttachemnt.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachemnt.storeOp         = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachemnt.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachemnt.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachemnt.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachemnt.finalLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment   = 0;
        colorAttachmentRef.layout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorResolverAttachment = {};
        colorResolverAttachment.flags           = 0;
        colorResolverAttachment.format          = Swapchain.SurfaceFormat.format;
        colorResolverAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
        colorResolverAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolverAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
        colorResolverAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolverAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorResolverAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        colorResolverAttachment.finalLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorResolverAttachmentRef = {};
        colorResolverAttachmentRef.attachment   = 2;
        colorResolverAttachmentRef.layout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth + stencil
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.flags           = 0;
        depthAttachment.format          = VK_FORMAT_D32_SFLOAT_S8_UINT;
        depthAttachment.samples         = sampleCount;
        depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment   = 1;
        depthAttachmentRef.layout       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.flags                   = 0;
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount    = 0;
        subpass.pInputAttachments       = nullptr;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorAttachmentRef;
        subpass.pResolveAttachments     = &colorResolverAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments    = nullptr;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass       = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass       = 0;
        dependency.srcStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask    = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags  = 0;

        VkAttachmentDescription attachmentDescs[3] = {
            colorAttachemnt,
            depthAttachment,
            colorResolverAttachment,
        };

        VkRenderPassCreateInfo renderPassCreateInfo = {};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.pNext              = nullptr;
        renderPassCreateInfo.flags              = 0;
        renderPassCreateInfo.attachmentCount    = VK_ARR_SIZE(attachmentDescs);
        renderPassCreateInfo.pAttachments       = &attachmentDescs[0];
        renderPassCreateInfo.subpassCount       = 1;
        renderPassCreateInfo.pSubpasses         = &subpass;
        renderPassCreateInfo.dependencyCount    = 1;
        renderPassCreateInfo.pDependencies      = &dependency;

        vkCreateRenderPass(Context.Device, &renderPassCreateInfo, nullptr, &Pipeline.RenderPass);
    }

    void CreateFramebuffers()
    {
        // Create framebuffers
        for (uint32_t i = 0; i < Swapchain.ImagesKhrCount; i++)
        {
            const VkImageView attachments[3] = {
                Swapchain.MsaaColorImageView,
                Swapchain.DepthStencilImageView,
                Swapchain.ImageViews[i]
            };

            VkFramebufferCreateInfo framebufferCreateInfo = {};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.flags             = 0;
            framebufferCreateInfo.renderPass        = Pipeline.RenderPass;
            framebufferCreateInfo.attachmentCount   = VK_ARR_SIZE(attachments);
            framebufferCreateInfo.pAttachments      = &attachments[0];
            framebufferCreateInfo.width             = Swapchain.Extent.width;
            framebufferCreateInfo.height            = Swapchain.Extent.height;
            framebufferCreateInfo.layers            = 1;

            vkCreateFramebuffer(Context.Device, &framebufferCreateInfo, nullptr, &Swapchain.Framebuffers[i]);
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
        vmaDestroyImage(Context.Allocator, Swapchain.MsaaColorImage, Swapchain.MsaaColorMem);

        // Destroy depth-stencil resources
        vkDestroyImageView(Context.Device, Swapchain.DepthStencilImageView, nullptr);
        vmaDestroyImage(Context.Allocator, Swapchain.DepthStencilImage, Swapchain.DepthStencilMem);

        VkSwapchainKHR oldSwapchain = Swapchain.Swapchain;
        CreateSwapchain(oldSwapchain);
        CreateFramebuffers();
        
        vkDestroySwapchainKHR(Context.Device, oldSwapchain, nullptr);
    }

    void CreateFrameInFlight()
    {
        // Each commnad buffer has its own command pool (vulkan best practices).
        // Every command pool in frame in flight has the same create info.
        VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.flags             = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolCreateInfo.queueFamilyIndex  = Context.GraphcisQueueFamilyIndex;

        for (size_t i = 0; i < MAX_FIF; i++)
        {
            VkCommandPool& cmdPool = Frame.CmdPool[i];
            vkCreateCommandPool(Context.Device, &cmdPoolCreateInfo, nullptr, &cmdPool);

            VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
            cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdBufferAllocInfo.commandPool           = cmdPool;
            cmdBufferAllocInfo.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmdBufferAllocInfo.commandBufferCount    = 1;
            vkAllocateCommandBuffers(Context.Device, &cmdBufferAllocInfo, &Frame.CmdBuffer[i]);

            VkSemaphoreCreateInfo semaphoreCreateInfo = {};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreCreateInfo.flags = 0;
            vkCreateSemaphore(Context.Device, &semaphoreCreateInfo, nullptr, &Frame.AcquiredImageSemaphores[i]);

            VkFenceCreateInfo fenceCreateInfo = {};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateFence(Context.Device, &fenceCreateInfo, nullptr, &Frame.SubmitFences[i]);
        }
    }

    void CreateStageBuffer()
    {
        VkPhysicalDeviceProperties physDeviceProperties = {};
        vkGetPhysicalDeviceProperties(Context.PhysDevice, &physDeviceProperties);

        VkBufferCreateInfo stageBufferCreateInfo = {};
        stageBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stageBufferCreateInfo.flags = 0;
        stageBufferCreateInfo.size  = VK_ALIGN(1024 * 1024 * 512, physDeviceProperties.limits.minMemoryMapAlignment); // @vulkan-best-practice 512mb are too much?
        stageBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        vmaCreateBuffer(Context.Allocator, &stageBufferCreateInfo, &allocCreateInfo, &Scene.StageBuffer, &Scene.StageAlloc, &Scene.StageAllocInfo);
    }

    void PrepareSwapchainImages()
    {
        VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(Frame.CmdBuffer[0], &cmdBufferBeginInfo);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.image                           = Swapchain.DepthStencilImage;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        vkCmdPipelineBarrier(Frame.CmdBuffer[0],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        vkEndCommandBuffer(Frame.CmdBuffer[0]);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &Frame.CmdBuffer[0];

        vkQueueSubmit(Context.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(Context.GraphicsQueue);
    }

    void CreatePipeline()
    {
        std::vector<char> vert_shader_code = FileSystem::ReadFile("../resources/shaders/indirect_vert.spv");
        std::vector<char> frag_shader_code = FileSystem::ReadFile("../resources/shaders/frag.spv");

        std::array<VkShaderModule, 2> shader_modules = {};

        VkShaderModuleCreateInfo vertex_module_create_info = {};
        vertex_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertex_module_create_info.flags = 0;
        vertex_module_create_info.codeSize = static_cast<uint32_t>(vert_shader_code.size());
        vertex_module_create_info.pCode = reinterpret_cast<const uint32_t*>(vert_shader_code.data());

        VkShaderModuleCreateInfo frag_module_create_info = {};
        frag_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        frag_module_create_info.flags = 0;
        frag_module_create_info.codeSize = static_cast<uint32_t>(frag_shader_code.size());
        frag_module_create_info.pCode = reinterpret_cast<const uint32_t*>(frag_shader_code.data());

        vkCreateShaderModule(context_.device, &vertex_module_create_info, nullptr, &shader_modules[0]);
        vkCreateShaderModule(context_.device, &frag_module_create_info, nullptr, &shader_modules[1]);

        VkPipelineShaderStageCreateInfo vert_stage_create_info = {};
        vert_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_stage_create_info.flags = 0;
        vert_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_stage_create_info.module = shader_modules[0];
        vert_stage_create_info.pName = "main";
        vert_stage_create_info.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo frag_stage_create_info = {};
        frag_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_stage_create_info.flags = 0;
        frag_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_stage_create_info.module = shader_modules[1];
        frag_stage_create_info.pName = "main";
        frag_stage_create_info.pSpecializationInfo = nullptr;

        const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
            vert_stage_create_info,
            frag_stage_create_info
        };

        const std::array<VkDynamicState, 2> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dyn_state_create_info = {};
        dyn_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn_state_create_info.flags = 0;
        dyn_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dyn_state_create_info.pDynamicStates = dynamic_states.data();

        std::array<VkVertexInputBindingDescription, 3> bind_descs = {};
        bind_descs[0].binding   = 0;
        bind_descs[0].stride    = sizeof(float) * 3;
        bind_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        bind_descs[1].binding   = 1;
        bind_descs[1].stride    = sizeof(float) * 3;
        bind_descs[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        bind_descs[2].binding   = 2;
        bind_descs[2].stride    = sizeof(float) * 2;
        bind_descs[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 3> attribute_descs = {};
        // Position
        attribute_descs[0].location     = 0;
        attribute_descs[0].binding      = 0;
        attribute_descs[0].format       = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descs[0].offset       = 0;
        // Normal
        attribute_descs[1].location     = 1;
        attribute_descs[1].binding      = 1;
        attribute_descs[1].format       = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descs[1].offset       = 0;
        // UV
        attribute_descs[2].location     = 2;
        attribute_descs[2].binding      = 2;
        attribute_descs[2].format       = VK_FORMAT_R32G32_SFLOAT;
        attribute_descs[2].offset       = 0;


        VkPipelineVertexInputStateCreateInfo vertext_input_info = {};
        vertext_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertext_input_info.flags = 0;
        vertext_input_info.vertexBindingDescriptionCount = bind_descs.size();
        vertext_input_info.pVertexBindingDescriptions = bind_descs.data();
        vertext_input_info.vertexAttributeDescriptionCount = attribute_descs.size();
        vertext_input_info.pVertexAttributeDescriptions = attribute_descs.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.pNext = nullptr;
        input_assembly_info.flags = 0;
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_info.primitiveRestartEnable = VK_FALSE;

        const VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchain_.extent.width),
            .height = static_cast<float>(swapchain_.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        const VkRect2D scissor = {
            .offset = {0, 0},
            .extent = swapchain_.extent,
        };

        VkPipelineViewportStateCreateInfo viewport_info = {};
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.flags = 0;
        viewport_info.viewportCount = 1;
        viewport_info.pViewports = &viewport;
        viewport_info.scissorCount = 1;
        viewport_info.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterization_info = {};
        rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_info.flags = 0;
        rasterization_info.depthClampEnable = VK_FALSE;
        rasterization_info.rasterizerDiscardEnable = VK_FALSE;
        rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_info.depthBiasEnable = VK_FALSE;
        rasterization_info.depthBiasConstantFactor = 0.0f;
        rasterization_info.depthBiasClamp = 0.0f;
        rasterization_info.depthBiasSlopeFactor = 0.0f;
        rasterization_info.lineWidth = 1.0f;

        constexpr VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_4_BIT;

        VkPipelineMultisampleStateCreateInfo multiple_sample_info = {};
        multiple_sample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multiple_sample_info.flags = 0;
        multiple_sample_info.rasterizationSamples = sample_count;
        multiple_sample_info.sampleShadingEnable = VK_TRUE;
        multiple_sample_info.minSampleShading = 1.0f;
        multiple_sample_info.pSampleMask = nullptr;
        multiple_sample_info.alphaToCoverageEnable = VK_FALSE;
        multiple_sample_info.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_info = {};
        color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_info.flags = 0;
        color_blend_info.logicOpEnable = VK_FALSE;
        color_blend_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_info.attachmentCount = 1;
        color_blend_info.pAttachments = &color_blend_attachment;
        color_blend_info.blendConstants[0] = 0.0f;
        color_blend_info.blendConstants[1] = 0.0f;
        color_blend_info.blendConstants[2] = 0.0f;
        color_blend_info.blendConstants[3] = 0.0f;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
        // Ubo
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // Ssbo
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // Bindless texture array
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount = 100;  // @todo: temporary constant max texture array.
        bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorBindingFlags, 3> binding_flags = {};
        binding_flags[0] = 0;   // ubo
        binding_flags[1] = 0;   // ssbo
        binding_flags[2] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo layout_flags = {};
        layout_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        layout_flags.bindingCount = binding_flags.size();
        layout_flags.pBindingFlags = binding_flags.data();

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_info.pNext = &layout_flags;
        layout_info.bindingCount = bindings.size();
        layout_info.pBindings = bindings.data();

        vkCreateDescriptorSetLayout(context_.device, &layout_info, nullptr, &pipeline_.descriptor_set_layout);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.flags = 0;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &pipeline_.descriptor_set_layout;
        pipeline_layout_create_info.pushConstantRangeCount = 0;
        pipeline_layout_create_info.pPushConstantRanges = nullptr;

        vkCreatePipelineLayout(context_.device, &pipeline_layout_create_info, nullptr, &pipeline_.pipeline_layout);

        VkStencilOpState stencil_op = {};
        stencil_op.failOp = VK_STENCIL_OP_KEEP;
        stencil_op.passOp = VK_STENCIL_OP_REPLACE;
        stencil_op.depthFailOp = VK_STENCIL_OP_KEEP;
        stencil_op.compareOp = VK_COMPARE_OP_ALWAYS;
        stencil_op.compareMask = 0xFF;
        stencil_op.writeMask = 0xFF;
        stencil_op.reference = 1;

        // Depth + Stencil
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
        depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
        depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
        depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_state_create_info.stencilTestEnable = VK_TRUE;
        depth_stencil_state_create_info.front = stencil_op;
        depth_stencil_state_create_info.back = stencil_op;
        depth_stencil_state_create_info.minDepthBounds = 0.0f;
        depth_stencil_state_create_info.maxDepthBounds = 1.0f;

        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
        graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphics_pipeline_create_info.flags = 0;
        graphics_pipeline_create_info.stageCount = 2;
        graphics_pipeline_create_info.pStages = shader_stages.data();
        graphics_pipeline_create_info.pVertexInputState = &vertext_input_info;
        graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_info;
        graphics_pipeline_create_info.pTessellationState = nullptr;
        graphics_pipeline_create_info.pViewportState = &viewport_info;
        graphics_pipeline_create_info.pRasterizationState = &rasterization_info;
        graphics_pipeline_create_info.pMultisampleState = &multiple_sample_info;
        graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
        graphics_pipeline_create_info.pColorBlendState = &color_blend_info;
        graphics_pipeline_create_info.pDynamicState = &dyn_state_create_info;
        graphics_pipeline_create_info.layout = pipeline_.pipeline_layout;
        graphics_pipeline_create_info.renderPass = pipeline_.render_pass;
        graphics_pipeline_create_info.subpass = 0;
        graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        graphics_pipeline_create_info.basePipelineIndex = -1;

        vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline_.physical_base_rendering_pipeline);

        std::array<VkDescriptorPoolSize, 3> pool_sizes = {};
        // Ubo
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = kMaxFifCount;
        // Ssbo
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_sizes[1].descriptorCount = kMaxFifCount;
        // Bindless texturing
        pool_sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[2].descriptorCount = 100 * kMaxFifCount;    // @todo: define max count of texture array.

        VkDescriptorPoolCreateInfo pool_create_info = {};
        pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_create_info.maxSets = kMaxFifCount;
        pool_create_info.poolSizeCount = pool_sizes.size();
        pool_create_info.pPoolSizes = pool_sizes.data();

        vkCreateDescriptorPool(context_.device, &pool_create_info, nullptr, &pipeline_.descriptor_pool);

        std::array<VkDescriptorSetLayout, 2> desc_set_layouts = {};
        desc_set_layouts[0] = pipeline_.descriptor_set_layout;
        desc_set_layouts[1] = pipeline_.descriptor_set_layout;

        VkDescriptorSetAllocateInfo desc_set_allocate_info = {};
        desc_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc_set_allocate_info.descriptorPool = pipeline_.descriptor_pool;
        desc_set_allocate_info.descriptorSetCount = kMaxFifCount;
        desc_set_allocate_info.pSetLayouts = desc_set_layouts.data();

        vkAllocateDescriptorSets(context_.device, &desc_set_allocate_info, &frame_data_.descriptor_sets[0]);

        for (size_t i = 0; i < kMaxFifCount; i++)
        {
            VkDescriptorBufferInfo ubo_info = {};
            ubo_info.buffer = frame_data_.ubo_buffer[i];
            ubo_info.offset = 0;
            ubo_info.range = sizeof(float) * 16 * 2;

            VkDescriptorBufferInfo ssbo_info = {};
            ssbo_info.buffer = frame_data_.ssbo_buffer[0];      // Use the same buffer since the geometry doesn't change (yet).
            ssbo_info.offset = 0;
            ssbo_info.range = sizeof(SsboObjectData);

            std::array<VkWriteDescriptorSet, 2> descriptor_sets = {};
            // Ubo
            descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_sets[0].dstSet = frame_data_.descriptor_sets[i];
            descriptor_sets[0].dstBinding = 0;
            descriptor_sets[0].dstArrayElement = 0;
            descriptor_sets[0].descriptorCount = 1;
            descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_sets[0].pBufferInfo = &ubo_info;
            // Ssbo
            descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_sets[1].dstSet = frame_data_.descriptor_sets[i];
            descriptor_sets[1].dstBinding = 1;
            descriptor_sets[1].dstArrayElement = 0;
            descriptor_sets[1].descriptorCount = 1;
            descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptor_sets[1].pBufferInfo = &ssbo_info;

            vkUpdateDescriptorSets(context_.device, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
        }

        vkDestroyShaderModule(context_.device, shader_modules[0], nullptr);
        vkDestroyShaderModule(context_.device, shader_modules[1], nullptr);
    }
}