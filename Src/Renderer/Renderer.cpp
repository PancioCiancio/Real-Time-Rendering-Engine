//
// Created by apant on 02/08/2025.
//

#include "Renderer.h"
#include "../FileSystem.h"
#include "MeshLoader.h"
#include "VkCommon.h"

// Must define this macro and include the header file in one and only one implementation file.
#define VOLK_IMPLEMENTATION
// ReSharper disable once CppUnusedIncludeDirective
#include <volk/volk.h>

#include <SDL2/SDL_vulkan.h>

namespace Renderer
{
// @todo just for testing purpose.
static uint32_t INDICES_COUNT = {};


VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
    void *pUserData)
{
    // ReSharper disable once CppDFAUnusedValue
    const char *severity = "";
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity = "[VERBOSE]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severity = "[INFO]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity = "[WARNING]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severity = "[ERROR]";
            break;
        default: severity = "";
            break;
    }

    // @TODO:	cover the message type as well....

    std::printf("[VK] %s %s\n", severity, callbackData->pMessage);

    return VK_FALSE;
}

constexpr VkDebugUtilsMessageSeverityFlagsEXT DEBUG_UTILS_MESSAGE_SEVERITY_FLAGS =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

constexpr VkDebugUtilsMessageTypeFlagsEXT DEBUG_UTILS_MESSAGE_TYPE_FLAGS =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

constexpr VkDebugUtilsMessengerCreateInfoEXT DEBUG_UTILS_MESSENGER_CREATE_INFO = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .pNext = nullptr,
    .flags = 0,
    .messageSeverity = DEBUG_UTILS_MESSAGE_SEVERITY_FLAGS,
    .messageType = DEBUG_UTILS_MESSAGE_TYPE_FLAGS,
    .pfnUserCallback = DebugCallback,
    .pUserData = nullptr,
};

void Renderer::Init()
{
    // Init the window class
    VK_CHECK((SDL_Init(SDL_INIT_VIDEO) == 0)
        ? VK_SUCCESS
        : VK_ERROR_UNKNOWN);

    window = SDL_CreateWindow(
        "Adro Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640,
        480,
        SDL_WINDOW_VULKAN);

    VK_CHECK(volkInitialize());

    InitInstance();
    InitSurface();
    InitDevice();
    InitSwapchain();
    InitOtherImages();
    InitUniformBuffer();
    InitCommand();
    InitBatch();
    InitRenderpass();
    InitFramebuffers();
    InitPipeline();

    PrepareDepthStencil();
}

void Renderer::Update(double delta_time)
{
    // Timing variables
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 last = 0;
    double deltaTime = 0;

    // Target frame rate (optional)
    constexpr int MAX_FPS = 60;
    constexpr double TARGET_FRAME_TIME = 1000.0 / MAX_FPS; // in milliseconds// Camera data

    uint32_t fifIndex = 0u;

    glm::vec3 cameraPos = {0.0f, .0f, -100.0f};
    glm::vec3 cameraPosNew = {0.0f, .0f, -100.0f};
    glm::vec3 cameraFront = {0.0f, 0.0f, 1.0f};
    glm::vec3 cameraUp = {0.0f, 1.0f, 0.0f};

    constexpr float P = 1.0f / 100.0f;
    constexpr float T = 0.396f;
    const float halfTime = -T / glm::log2(P);

    VkPipeline chosenPipeline = pipeline;

    bool stillRunning = true;
    while (stillRunning)
    {
        constexpr float CAMERA_MOVE_SPEED = 200.639f;
        last = now;
        now = SDL_GetPerformanceCounter();

        // Calculate delta time in seconds
        deltaTime = static_cast<double>(now - last) / static_cast<double>(
                        SDL_GetPerformanceFrequency());

        const float cameraLerpAlpha = 1.0f - glm::pow(
                                          2.0f, -static_cast<float>(deltaTime) / halfTime);

        SDL_Event event = {};

        // Input state
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT: stillRunning = false;
                    break;

                default:
                    // Do nothing.
                    break;
            }
        }

        const Uint8 *keyStates = SDL_GetKeyboardState(nullptr);

        if (keyStates[SDL_SCANCODE_W])
        {
            cameraPosNew += CAMERA_MOVE_SPEED * static_cast<float>(deltaTime) * cameraFront;
        }

        if (keyStates[SDL_SCANCODE_S])
        {
            cameraPosNew -= CAMERA_MOVE_SPEED * static_cast<float>(deltaTime) * cameraFront;
        }

        if (keyStates[SDL_SCANCODE_A])
        {
            cameraPosNew -= glm::normalize(glm::cross(cameraFront, cameraUp)) *
                CAMERA_MOVE_SPEED * static_cast<float>(deltaTime);
        }

        if (keyStates[SDL_SCANCODE_D])
        {
            cameraPosNew += glm::normalize(glm::cross(cameraFront, cameraUp)) *
                CAMERA_MOVE_SPEED * static_cast<float>(deltaTime);
        }

        if (keyStates[SDL_SCANCODE_Q])
        {
            chosenPipeline = pipelineWireframe;
        }
        else if (keyStates[SDL_SCANCODE_E])
        {
            chosenPipeline = pipeline;
        }

        cameraPos = glm::mix(cameraPos, cameraPosNew, cameraLerpAlpha);

        vkWaitForFences(device, 1, &framesInFlight.submitFence[fifIndex],
            VK_TRUE, UINT64_MAX);

        uint32_t next_image = 0u;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
            framesInFlight.acquiredImageSemaphore[fifIndex],
            VK_NULL_HANDLE, &next_image));

        vkResetFences(device, 1, &framesInFlight.submitFence[fifIndex]);
        vkResetCommandPool(device, framesInFlight.commandPool[fifIndex], 0);

        PerFrameDataCpu uBuffer = {
            glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp),
            glm::perspectiveRH_ZO(glm::radians(45.0f),
                static_cast<float>(surfaceCapabilities.currentExtent.width) /
                static_cast<float>(surfaceCapabilities.currentExtent.height),
                0.1f, 10000.0f),
        };

        // Flip vulkan Y-axis
        uBuffer.projection[1][1] *= -1;

        VK_CHECK(vkMapMemory(device, uniformBufferFrames.memory[fifIndex], 0,
            sizeof(uBuffer), 0, &uniformBufferFrames.data_mapped[fifIndex]));

        constexpr size_t uBufferSize = sizeof(PerFrameDataCpu);

        memcpy(uniformBufferFrames.data_mapped[fifIndex], &uBuffer, uBufferSize);

        vkUnmapMemory(device, uniformBufferFrames.memory[fifIndex]);

        VkCommandBufferBeginInfo commandBufferBeginInfo = {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;

        VK_CHECK(vkBeginCommandBuffer(framesInFlight.commandBuffer[fifIndex],
            &commandBufferBeginInfo));

        vkCmdBindDescriptorSets(framesInFlight.commandBuffer[fifIndex],
            VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
            &uniformBufferFrames.descriptor_sets[fifIndex], 0, nullptr);

        constexpr VkClearValue CLEAR_VALUES[2] = {
            {
                .color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}
            },
            {
                .depthStencil = {1.0f, 0},
            }
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = presentationFrames.framebuffer[next_image];
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = surfaceCapabilities.currentExtent;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = &CLEAR_VALUES[0];

        vkCmdBeginRenderPass(framesInFlight.commandBuffer[fifIndex],
            &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(framesInFlight.commandBuffer[fifIndex],
            VK_PIPELINE_BIND_POINT_GRAPHICS, chosenPipeline);

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(surfaceCapabilities.currentExtent.width);
        viewport.height = static_cast<float>(surfaceCapabilities.currentExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(framesInFlight.commandBuffer[fifIndex], 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = surfaceCapabilities.currentExtent;

        vkCmdSetScissor(framesInFlight.commandBuffer[fifIndex], 0, 1, &scissor);

        const VkBuffer bindsBuffer[] = {
            batch.position_buffer,
            batch.color_buffer,
            batch.normal_buffer,
        };

        constexpr VkDeviceSize OFFSETS[] = {
            0,
            0,
            0
        };

        vkCmdBindVertexBuffers(framesInFlight.commandBuffer[fifIndex], 0, 3, &bindsBuffer[0],
            &OFFSETS[0]);

        vkCmdBindIndexBuffer(framesInFlight.commandBuffer[fifIndex],
            batch.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(framesInFlight.commandBuffer[fifIndex], INDICES_COUNT,
            1, 0, 0, 0);

        vkCmdEndRenderPass(framesInFlight.commandBuffer[fifIndex]);

        VK_CHECK(vkEndCommandBuffer(framesInFlight.commandBuffer[fifIndex]));

        VkSemaphore wait_semaphores[] = {
            framesInFlight.acquiredImageSemaphore[fifIndex]
        };

        VkPipelineStageFlags wait_stages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };

        VkSemaphore signal_semaphores[] = {
            presentationFrames.renderFinishedSemaphore[next_image]
        };

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &framesInFlight.commandBuffer[fifIndex];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, framesInFlight.submitFence[fifIndex]));

        VkResult result = {};
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &presentationFrames.renderFinishedSemaphore[next_image];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &next_image;
        present_info.pResults = &result;

        // @todo: cannot present the image if the window is minimized.
        VK_CHECK(vkQueuePresentKHR(queue, &present_info));

        // --- Your game update & render logic here ---
        // Example: updateGame(deltaTime); render();

        // Frame limiting: Sleep if frame is faster than target frame time
        if (deltaTime < TARGET_FRAME_TIME)
        {
            SDL_Delay(static_cast<Uint32>(TARGET_FRAME_TIME - deltaTime));
        }

        fifIndex = (fifIndex + 1) % FramesInFlightType::MAX_FIF_COUNT;
    }
}

void Renderer::Teardown() const
{
    VK_CHECK(
        vkDeviceWaitIdle(device));

    vkDestroyDescriptorPool(
        device,
        descriptorPool,
        nullptr);
    vkDestroyDescriptorSetLayout(
        device,
        descriptorSetLayout,
        nullptr);
    vkDestroyRenderPass(
        device,
        renderPass,
        nullptr);
    vkDestroyPipelineLayout(
        device,
        pipelineLayout,
        nullptr);
    vkDestroyPipeline(
        device,
        pipeline,
        nullptr);
    vkDestroyPipeline(
        device,
        pipelineWireframe,
        nullptr);

    vkDestroyBuffer(
        device,
        batch.color_buffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.color_mem,
        nullptr);

    vkDestroyBuffer(
        device,
        batch.normal_buffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.normal_mem,
        nullptr);

    vkDestroyBuffer(
        device,
        batch.position_buffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.position_mem,
        nullptr);

    vkDestroyBuffer(
        device,
        batch.index_buffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.index_mem,
        nullptr);

    for (const VkDeviceMemory &memory : uniformBufferFrames.memory)
    {
        vkFreeMemory(
            device,
            memory,
            nullptr);
    }

    for (const VkBuffer &buffer : uniformBufferFrames.buffers)
    {
        vkDestroyBuffer(
            device,
            buffer,
            nullptr);
    }

    // Multisaple image
    vkDestroyImageView(
        device,
        framebufferSampleImageView,
        nullptr);
    vkDestroyImage(
        device,
        framebufferSampleImage,
        nullptr);
    vkFreeMemory(
        device,
        framebufferSampleImageMemory,
        nullptr);

    // Depth stencil
    vkDestroyImageView(
        device,
        depthStencilImageView,
        nullptr);
    vkDestroyImage(
        device,
        depthStencilImage,
        nullptr);
    vkFreeMemory(
        device,
        depthStencilMemory,
        nullptr);

    for (uint32_t i = 0; i < surfaceCapabilities.minImageCount; i++)
    {
        vkDestroyFramebuffer(
            device,
            presentationFrames.framebuffer[i],
            nullptr);

        vkDestroyImageView(
            device,
            presentationFrames.imageView[i],
            nullptr);

        vkDestroySemaphore(
            device,
            presentationFrames.renderFinishedSemaphore[i],
            nullptr);
    }

    for (const VkSemaphore &framesInFlightSemaphore : framesInFlight.acquiredImageSemaphore)
    {
        vkDestroySemaphore(device, framesInFlightSemaphore, nullptr);
    }

    for (const VkFence &framesInFlightFence : framesInFlight.submitFence)
    {
        vkDestroyFence(device, framesInFlightFence, nullptr);
    }

    vkDestroySwapchainKHR(
        device,
        swapchain,
        nullptr);

    for (const VkCommandPool &framesInFlightCommandPool : framesInFlight.commandPool)
    {
        vkDestroyCommandPool(device, framesInFlightCommandPool, nullptr);
    }

    vkDestroyDevice(
        device,
        nullptr);
    vkDestroySurfaceKHR(
        instance,
        surface,
        nullptr);
    vkDestroyDebugUtilsMessengerEXT(
        instance,
        debugMessenger,
        nullptr);
    vkDestroyInstance(
        instance,
        nullptr);

    SDL_DestroyWindow(
        window);
    SDL_Quit();
}

void Renderer::InitInstance()
{
    VK_CHECK(volkInitialize());

    // @todo:	calculate the layers count from the array.
    const char *requestedLayers[] = {"VK_LAYER_KHRONOS_validation"};

    // @todo:	calculate the extension count from the array.
    const char *requestedExtensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                         VK_KHR_SURFACE_EXTENSION_NAME,
                                         VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Adro";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = &DEBUG_UTILS_MESSENGER_CREATE_INFO;
    // @TODO: turn it on only if needed (debug build).
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = std::size(requestedLayers);
    instanceCreateInfo.ppEnabledLayerNames = &requestedLayers[0];
    instanceCreateInfo.enabledExtensionCount = std::size(requestedExtensions);
    instanceCreateInfo.ppEnabledExtensionNames = &requestedExtensions[0];

    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    volkLoadInstance(instance);

    vkCreateDebugUtilsMessengerEXT(instance, &DEBUG_UTILS_MESSENGER_CREATE_INFO,
        nullptr, &debugMessenger);
}

void Renderer::InitSurface()
{
    // @todo use the vulkan call and not the sdl one.
    VK_CHECK(
        SDL_Vulkan_CreateSurface(
            window,
            instance,
            &surface)
        ? VK_SUCCESS
        : VK_ERROR_INITIALIZATION_FAILED);
}

void Renderer::InitDevice()
{
    VkPhysicalDeviceFeatures gpuRequiredFeatures = {};
    gpuRequiredFeatures.geometryShader = VK_TRUE;
    gpuRequiredFeatures.tessellationShader = VK_TRUE;
    gpuRequiredFeatures.multiDrawIndirect = VK_TRUE;
    gpuRequiredFeatures.fillModeNonSolid = VK_TRUE;
    gpuRequiredFeatures.sampleRateShading = VK_TRUE;
    gpuRequiredFeatures.samplerAnisotropy = VK_TRUE;

    // @todo:	calculate the device extension count from the array.
    const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    QueryGpu(instance, gpuRequiredFeatures, std::size(deviceExtensions),
        deviceExtensions, &gpu);

    QueryQueueFamily(gpu, VK_QUEUE_GRAPHICS_BIT, true, 0, nullptr,
        &queueFamilyIndex);

    constexpr float QUEUE_PRIORITIES = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.flags = 0;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &QUEUE_PRIORITIES;

    constexpr uint32_t QUEUE_FAMILY_COUNT = 1;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = QUEUE_FAMILY_COUNT;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = std::size(deviceExtensions);
    deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensions[0];
    deviceCreateInfo.pEnabledFeatures = &gpuRequiredFeatures;

    VK_CHECK(vkCreateDevice(gpu, &deviceCreateInfo, nullptr, &device));

    volkLoadDevice(device);

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
}

void Renderer::InitSwapchain()
{
    VkFormat required_surface_formats[1] = {
        VK_FORMAT_R8G8B8A8_SRGB,
    };

    vk_query_surface_format(
        gpu,
        surface,
        1,
        required_surface_formats,
        &surfaceFormat);

    vk_query_surface_capabilities(
        gpu,
        surface,
        &surfaceCapabilities);

    vk_create_swapchain(
        device,
        surface,
        &surfaceFormat,
        &surfaceCapabilities,
        // At this point is VK_NULL_HANDLE
        swapchain,
        nullptr,
        &swapchain);

    presentationFrames.image = new VkImage[surfaceCapabilities.minImageCount];
    presentationFrames.imageView = new VkImageView[surfaceCapabilities.minImageCount];
    presentationFrames.framebuffer = new VkFramebuffer[surfaceCapabilities.minImageCount];
    presentationFrames.renderFinishedSemaphore = new VkSemaphore[surfaceCapabilities.minImageCount];

    vk_query_swapchain_images(
        device,
        swapchain,
        &presentationFrames.image[0]);

    // Create the view of the swapchain images
    for (uint32_t i = 0; i < surfaceCapabilities.minImageCount; i++)
    {
        vk_create_image_view(
            device,
            presentationFrames.image[i],
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            surfaceFormat.format,
            {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            },
            nullptr,
            &presentationFrames.imageView[i]);
    }

    for (uint32_t i = 0; i < surfaceCapabilities.minImageCount; i++)
    {
        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_create_info.flags = 0;

        VK_CHECK(
            vkCreateSemaphore(
                device,
                &semaphore_create_info,
                nullptr,
                &presentationFrames.renderFinishedSemaphore[i]));
    }
}

void Renderer::InitOtherImages()
{
    VkSampleCountFlagBits sample_counts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(
        gpu,
        &sample_counts);

    vk_create_image(
        device,
        gpu,
        VK_IMAGE_TYPE_2D,
        surfaceFormat.format,
        {
            surfaceCapabilities.currentExtent.width,
            surfaceCapabilities.currentExtent.height,
            1
        },
        sample_counts,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        nullptr,
        &framebufferSampleImage,
        &framebufferSampleImageMemory);

    vk_create_image_view(
        device,
        framebufferSampleImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        surfaceFormat.format,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },
        nullptr,
        &framebufferSampleImageView);

    // Depth + Stencil

    constexpr VkFormat depth_stencil_format_requested[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };

    vk_query_supported_format(
        gpu,
        4,
        &depth_stencil_format_requested[0],
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        &depthStencilFormat);

    vk_create_image(
        device,
        gpu,
        VK_IMAGE_TYPE_2D,
        depthStencilFormat,
        {surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.width, 1},
        sample_counts,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        nullptr,
        &depthStencilImage,
        &depthStencilMemory);

    vk_create_image_view(
        device,
        depthStencilImage,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        depthStencilFormat,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },
        nullptr,
        &depthStencilImageView);
}

void Renderer::InitCommand()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                  VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    for (size_t i = 0; i < FramesInFlightType::MAX_FIF_COUNT; i++)
    {
        VkCommandPool &fifCommandPool = framesInFlight.commandPool[i];

        VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr,
            &fifCommandPool));

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = fifCommandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo,
            &framesInFlight.commandBuffer[i]));

        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.flags = 0;

        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr,
            &framesInFlight.acquiredImageSemaphore[i]));

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr,
            &framesInFlight.submitFence[i]));
    }
}


void Renderer::InitBatch()
{
    MeshLoader::Load(
        "../Resources/Meshes/SM_Behemoth.fbx",
        &batchData);

    INDICES_COUNT = batchData.indices.size();

    std::vector<glm::vec4> default_colors(
        batchData.position.size(),
        glm::vec4(
            .36f,
            .36f,
            .5f,
            1.0f));
    batchData.color = default_colors;

    const size_t position_buffer_size = sizeof(glm::vec3) * batchData.position.size();
    vk_create_buffer(
        device,
        gpu,
        position_buffer_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.position_buffer,
        &batch.position_mem);

    void *position_data = nullptr;
    VK_CHECK(
        vkMapMemory(
            device,
            batch.position_mem,
            0,
            position_buffer_size,
            0,
            &position_data));

    memcpy(
        position_data,
        &batchData.position[0],
        position_buffer_size);

    vkUnmapMemory(
        device,
        batch.position_mem);

    const size_t normal_buffer_size = sizeof(glm::vec3) * batchData.normals.size();
    vk_create_buffer(
        device,
        gpu,
        normal_buffer_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.normal_buffer,
        &batch.normal_mem);

    void *normal_data = nullptr;
    VK_CHECK(
        vkMapMemory(
            device,
            batch.normal_mem,
            0,
            normal_buffer_size,
            0,
            &normal_data));

    memcpy(
        normal_data,
        &batchData.normals[0],
        normal_buffer_size);

    vkUnmapMemory(
        device,
        batch.normal_mem);

    const size_t color_buffer_size = sizeof(glm::vec4) * batchData.color.size();
    vk_create_buffer(
        device,
        gpu,
        color_buffer_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.color_buffer,
        &batch.color_mem);

    void *color_data = nullptr;
    VK_CHECK(vkMapMemory(device, batch.color_mem, 0, color_buffer_size, 0, &color_data));

    memcpy(color_data, &batchData.color[0], color_buffer_size);

    vkUnmapMemory(device, batch.color_mem);

    const size_t index_buffer_size = sizeof(uint32_t) * batchData.indices.size();
    vk_create_buffer(device, gpu, index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.index_buffer, &batch.index_mem);

    void *index_data = nullptr;
    VK_CHECK(
        vkMapMemory(
            device,
            batch.index_mem,
            0,
            index_buffer_size,
            0,
            &index_data));

    memcpy(
        index_data,
        &batchData.indices[0],
        index_buffer_size);

    vkUnmapMemory(
        device,
        batch.index_mem);
}

void Renderer::InitFramebuffers() const
{
    for (size_t i = 0; i < surfaceCapabilities.minImageCount; i++)
    {
        const VkImageView attachments[3] = {
            framebufferSampleImageView, // Multisample
            depthStencilImageView,
            presentationFrames.imageView[i], // Multisample resolver to 1 sample.
        };

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = renderPass,
            .attachmentCount = 3,
            .pAttachments = &attachments[0],
            .width = surfaceCapabilities.currentExtent.width,
            .height = surfaceCapabilities.currentExtent.height,
            .layers = 1,
        };

        VK_CHECK(
            vkCreateFramebuffer(
                device,
                &framebuffer_info,
                nullptr,
                &presentationFrames.framebuffer[i]));
    }
}

void Renderer::InitRenderpass()
{
    VkSampleCountFlagBits sample_counts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(
        gpu,
        &sample_counts);

    VkAttachmentDescription color_attachment = {};
    color_attachment.flags = 0;
    color_attachment.format = surfaceFormat.format;
    color_attachment.samples = sample_counts;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription color_attachment_resolve = {};
    color_attachment_resolve.flags = 0;
    color_attachment_resolve.format = surfaceFormat.format;
    color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_resolve_ref = {};
    color_attachment_resolve_ref.attachment = 2;
    color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth + stencil
    VkAttachmentDescription depth_attachment = {};
    depth_attachment.flags = 0;
    depth_attachment.format = depthStencilFormat;
    depth_attachment.samples = sample_counts;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_reference = {};
    depth_attachment_reference.attachment = 1;
    depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pResolveAttachments = &color_attachment_resolve_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_reference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    // @TODO: I don't get what all these parameters mean.
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    constexpr uint32_t attachment_desc_count = 3;
    const VkAttachmentDescription attachment_descs[attachment_desc_count] = {
        color_attachment,
        depth_attachment,
        color_attachment_resolve,
    };

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.pNext = nullptr;
    render_pass_create_info.flags = 0;
    render_pass_create_info.attachmentCount = attachment_desc_count;
    render_pass_create_info.pAttachments = &attachment_descs[0];
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &renderPass));
}

void Renderer::InitUniformBuffer()
{
    for (uint32_t i = 0; i < FramesInFlightType::MAX_FIF_COUNT; i++)
    {
        constexpr VkDeviceSize buffer_size = sizeof(PerFrameDataCpu);

        vk_create_buffer(device, gpu, buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nullptr,
            &uniformBufferFrames.buffers[i], &uniformBufferFrames.memory[i]);
    }
}

void Renderer::InitPipeline()
{
    auto vert_shader_code = FileSystem::ReadFile(
        "../Resources/Shaders/vert.spv");
    auto frag_shader_code = FileSystem::ReadFile(
        "../Resources/Shaders/frag.spv");

    VkShaderModule shader_modules[2] = {};

    VkShaderModuleCreateInfo vert_module_info = {};
    vert_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_module_info.flags = 0;
    vert_module_info.codeSize = static_cast<uint32_t>(vert_shader_code.size());
    vert_module_info.pCode = reinterpret_cast<const uint32_t *>(vert_shader_code.data());

    VK_CHECK(
        vkCreateShaderModule(
            device,
            &vert_module_info,
            nullptr,
            &shader_modules[0]));

    VkShaderModuleCreateInfo frag_module_info = {};
    frag_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_module_info.flags = 0;
    frag_module_info.codeSize = static_cast<uint32_t>(frag_shader_code.size());
    frag_module_info.pCode = reinterpret_cast<const uint32_t *>(frag_shader_code.data());

    VK_CHECK(vkCreateShaderModule(device, &frag_module_info, nullptr, &shader_modules[1]));

    VkPipelineShaderStageCreateInfo vert_stage_info = {};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.flags = 0;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = shader_modules[0];
    vert_stage_info.pName = "main";
    vert_stage_info.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo frag_stage_info = {};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.flags = 0;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = shader_modules[1];
    frag_stage_info.pName = "main";
    frag_stage_info.pSpecializationInfo = nullptr;

    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        vert_stage_info,
        frag_stage_info
    };

    const std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.flags = 0;
    dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state_create_info.pDynamicStates = dynamic_states.data();

    constexpr VkVertexInputBindingDescription bind_descs[] = {
        // position
        {
            .binding = 0,
            .stride = sizeof(glm::vec3),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        },
        // color.
        {
            .binding = 1,
            .stride = sizeof(glm::vec4),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        },
        // normal.
        {
            .binding = 2,
            .stride = sizeof(glm::vec3),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }
    };

    constexpr VkVertexInputAttributeDescription attribute_description[3] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        },
        {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0
        },
        {
            .location = 2,
            .binding = 2,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        }
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.flags = 0;
    vertex_input_info.vertexBindingDescriptionCount = 3;
    vertex_input_info.pVertexBindingDescriptions = &bind_descs[0];
    vertex_input_info.vertexAttributeDescriptionCount = 3;
    vertex_input_info.pVertexAttributeDescriptions = &attribute_description[0];

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.pNext = nullptr;
    input_assembly_info.flags = 0;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    const VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(surfaceCapabilities.currentExtent.width),
        .height = static_cast<float>(surfaceCapabilities.currentExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D scissor = {
        .offset = {0, 0},
        .extent = surfaceCapabilities.currentExtent,
    };

    // ReSharper disable once CppVariableCanBeMadeConstexpr
    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.flags = 0;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_info = {};
    rasterization_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
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

    VkSampleCountFlagBits sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(gpu, &sampleCounts);

    VkPipelineMultisampleStateCreateInfo multisample_info = {};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.flags = 0;
    multisample_info.rasterizationSamples = sampleCounts;
    multisample_info.sampleShadingEnable = VK_TRUE;
    multisample_info.minSampleShading = 1.0f;
    multisample_info.pSampleMask = nullptr;
    multisample_info.alphaToCoverageEnable = VK_FALSE;
    multisample_info.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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

    VkDescriptorSetLayoutBinding u_buffer_set_binding = {};
    u_buffer_set_binding.binding = 0;
    u_buffer_set_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    u_buffer_set_binding.descriptorCount = 1;
    u_buffer_set_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    u_buffer_set_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &u_buffer_set_binding;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
        &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.flags = 0;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptorSetLayout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
        &pipelineLayout));

    // depth + stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
    depth_stencil_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
    depth_stencil_state_create_info.front = {};
    depth_stencil_state_create_info.back = {};
    depth_stencil_state_create_info.minDepthBounds = 0.0f;
    depth_stencil_state_create_info.maxDepthBounds = 1.0f;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shaderStages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pTessellationState = nullptr;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterization_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = &depth_stencil_state_create_info;
    pipeline_info.pColorBlendState = &color_blend_info;
    pipeline_info.pDynamicState = &dynamic_state_create_info;
    pipeline_info.layout = pipelineLayout;
    pipeline_info.renderPass = renderPass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkPipelineRasterizationStateCreateInfo rasterization_info_wireframe = {};
    rasterization_info_wireframe.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info_wireframe.flags = 0;
    rasterization_info_wireframe.depthClampEnable = VK_FALSE;
    rasterization_info_wireframe.rasterizerDiscardEnable = VK_FALSE;
    rasterization_info_wireframe.polygonMode = VK_POLYGON_MODE_LINE;
    rasterization_info_wireframe.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_info_wireframe.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_info_wireframe.depthBiasEnable = VK_FALSE;
    rasterization_info_wireframe.depthBiasConstantFactor = 0.0f;
    rasterization_info_wireframe.depthBiasClamp = 0.0f;
    rasterization_info_wireframe.depthBiasSlopeFactor = 0.0f;
    rasterization_info_wireframe.lineWidth = 1.0f;

    VkGraphicsPipelineCreateInfo pipeline_info_wireframe = {};
    pipeline_info_wireframe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info_wireframe.flags = 0;
    pipeline_info_wireframe.stageCount = 2;
    pipeline_info_wireframe.pStages = shaderStages;
    pipeline_info_wireframe.pVertexInputState = &vertex_input_info;
    pipeline_info_wireframe.pInputAssemblyState = &input_assembly_info;
    pipeline_info_wireframe.pTessellationState = nullptr;
    pipeline_info_wireframe.pViewportState = &viewport_info;
    pipeline_info_wireframe.pRasterizationState = &rasterization_info_wireframe;
    pipeline_info_wireframe.pMultisampleState = &multisample_info;
    pipeline_info_wireframe.pDepthStencilState = &depth_stencil_state_create_info;
    pipeline_info_wireframe.pColorBlendState = &color_blend_info;
    pipeline_info_wireframe.pDynamicState = &dynamic_state_create_info;
    pipeline_info_wireframe.layout = pipelineLayout;
    pipeline_info_wireframe.renderPass = renderPass;
    pipeline_info_wireframe.subpass = 0;
    pipeline_info_wireframe.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info_wireframe.basePipelineIndex = -1;

    VkGraphicsPipelineCreateInfo pipeline_infos[] = {pipeline_info,
                                                     pipeline_info_wireframe};

    VkPipeline pipelines[] = {pipeline, pipelineWireframe};

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 2, &pipeline_infos[0],
        nullptr, &pipelines[0]));

    pipeline = pipelines[0];
    pipelineWireframe = pipelines[1];

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = FramesInFlightType::MAX_FIF_COUNT;

    VkDescriptorPoolCreateInfo pool_create_info = {};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.maxSets = FramesInFlightType::MAX_FIF_COUNT;
    pool_create_info.poolSizeCount = 1;
    pool_create_info.pPoolSizes = &poolSize;

    VK_CHECK(vkCreateDescriptorPool(device, &pool_create_info, nullptr,
        &descriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(FramesInFlightType::MAX_FIF_COUNT,
        descriptorSetLayout);

    VkDescriptorSetAllocateInfo set_allocate_info = {};
    set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_allocate_info.descriptorPool = descriptorPool;
    set_allocate_info.descriptorSetCount = FramesInFlightType::MAX_FIF_COUNT;
    set_allocate_info.pSetLayouts = &layouts[0];

    VK_CHECK(vkAllocateDescriptorSets(device, &set_allocate_info,
        &uniformBufferFrames.descriptor_sets[0]));

    for (size_t i = 0; i < FramesInFlightType::MAX_FIF_COUNT; i++)
    {
        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = uniformBufferFrames.buffers[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(PerFrameDataCpu);

        VkWriteDescriptorSet descriptor_set = {};
        descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_set.dstSet = uniformBufferFrames.descriptor_sets[i];
        descriptor_set.dstBinding = 0;
        descriptor_set.dstArrayElement = 0;
        descriptor_set.descriptorCount = 1;
        descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(device, 1, &descriptor_set, 0, nullptr);
    }

    vkDestroyShaderModule(device, shader_modules[0], nullptr);
    vkDestroyShaderModule(device, shader_modules[1], nullptr);
}

void Renderer::PrepareDepthStencil()
{
    VkCommandBufferBeginInfo command_buffer_begin_info = {};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(framesInFlight.commandBuffer[0], &command_buffer_begin_info);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.image = depthStencilImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(framesInFlight.commandBuffer[0],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(framesInFlight.commandBuffer[0]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &framesInFlight.commandBuffer[0];

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
}
} // Renderer
