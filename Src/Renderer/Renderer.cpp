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
            &uniformBufferFrames.descriptorSets[fifIndex], 0, nullptr);

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
            batch.positionBuffer,
            batch.colorBuffer,
            batch.normalBuffer,
        };

        constexpr VkDeviceSize OFFSETS[] = {
            0,
            0,
            0
        };

        vkCmdBindVertexBuffers(framesInFlight.commandBuffer[fifIndex], 0, 3, &bindsBuffer[0],
            &OFFSETS[0]);

        vkCmdBindIndexBuffer(framesInFlight.commandBuffer[fifIndex],
            batch.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(framesInFlight.commandBuffer[fifIndex], INDICES_COUNT,
            1, 0, 0, 0);

        vkCmdEndRenderPass(framesInFlight.commandBuffer[fifIndex]);

        VK_CHECK(vkEndCommandBuffer(framesInFlight.commandBuffer[fifIndex]));

        VkSemaphore waitSemaphores[] = {
            framesInFlight.acquiredImageSemaphore[fifIndex]
        };

        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };

        VkSemaphore signal_semaphores[] = {
            presentationFrames.renderFinishedSemaphore[next_image]
        };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &framesInFlight.commandBuffer[fifIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signal_semaphores;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, framesInFlight.submitFence[fifIndex]));

        VkResult result = {};
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &presentationFrames.renderFinishedSemaphore[next_image];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &next_image;
        presentInfo.pResults = &result;

        // @todo: cannot present the image if the window is minimized.
        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

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
        batch.colorBuffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.colorMem,
        nullptr);

    vkDestroyBuffer(
        device,
        batch.normalBuffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.normalMem,
        nullptr);

    vkDestroyBuffer(
        device,
        batch.positionBuffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.positionMem,
        nullptr);

    vkDestroyBuffer(
        device,
        batch.indexBuffer,
        nullptr);
    vkFreeMemory(
        device,
        batch.indexMem,
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
    VkFormat requiredSurfaceFormats[1] = {
        VK_FORMAT_R8G8B8A8_SRGB,
    };

    vk_query_surface_format(
        gpu,
        surface,
        1,
        requiredSurfaceFormats,
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
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.flags = 0;

        VK_CHECK(
            vkCreateSemaphore(
                device,
                &semaphoreCreateInfo,
                nullptr,
                &presentationFrames.renderFinishedSemaphore[i]));
    }
}

void Renderer::InitOtherImages()
{
    VkSampleCountFlagBits sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(
        gpu,
        &sampleCounts);

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
        sampleCounts,
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

    constexpr VkFormat depthStencilFormatsRequested[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };

    vk_query_supported_format(
        gpu,
        4,
        &depthStencilFormatsRequested[0],
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        &depthStencilFormat);

    vk_create_image(
        device,
        gpu,
        VK_IMAGE_TYPE_2D,
        depthStencilFormat,
        {surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.width, 1},
        sampleCounts,
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

    std::vector<glm::vec4> defaultColors(
        batchData.position.size(),
        glm::vec4(
            .36f,
            .36f,
            .5f,
            1.0f));
    batchData.color = defaultColors;

    const size_t positionBufferSize = sizeof(glm::vec3) * batchData.position.size();
    vk_create_buffer(
        device,
        gpu,
        positionBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.positionBuffer,
        &batch.positionMem);

    void *positionData = nullptr;
    VK_CHECK(
        vkMapMemory(
            device,
            batch.positionMem,
            0,
            positionBufferSize,
            0,
            &positionData));

    memcpy(
        positionData,
        &batchData.position[0],
        positionBufferSize);

    vkUnmapMemory(
        device,
        batch.positionMem);

    const size_t normalBufferSize = sizeof(glm::vec3) * batchData.normals.size();
    vk_create_buffer(
        device,
        gpu,
        normalBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.normalBuffer,
        &batch.normalMem);

    void *normal_data = nullptr;
    VK_CHECK(
        vkMapMemory(
            device,
            batch.normalMem,
            0,
            normalBufferSize,
            0,
            &normal_data));

    memcpy(
        normal_data,
        &batchData.normals[0],
        normalBufferSize);

    vkUnmapMemory(
        device,
        batch.normalMem);

    const size_t colorBufferSize = sizeof(glm::vec4) * batchData.color.size();
    vk_create_buffer(
        device,
        gpu,
        colorBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.colorBuffer,
        &batch.colorMem);

    void *colorData = nullptr;
    VK_CHECK(vkMapMemory(device, batch.colorMem, 0, colorBufferSize, 0, &colorData));

    memcpy(colorData, &batchData.color[0], colorBufferSize);

    vkUnmapMemory(device, batch.colorMem);

    const size_t indexBufferSize = sizeof(uint32_t) * batchData.indices.size();
    vk_create_buffer(device, gpu, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.indexBuffer, &batch.indexMem);

    void *index_data = nullptr;
    VK_CHECK(
        vkMapMemory(
            device,
            batch.indexMem,
            0,
            indexBufferSize,
            0,
            &index_data));

    memcpy(
        index_data,
        &batchData.indices[0],
        indexBufferSize);

    vkUnmapMemory(
        device,
        batch.indexMem);
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

        VkFramebufferCreateInfo framebufferInfo = {
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
                &framebufferInfo,
                nullptr,
                &presentationFrames.framebuffer[i]));
    }
}

void Renderer::InitRenderpass()
{
    VkSampleCountFlagBits sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(
        gpu,
        &sampleCounts);

    VkAttachmentDescription color_attachment = {};
    color_attachment.flags = 0;
    color_attachment.format = surfaceFormat.format;
    color_attachment.samples = sampleCounts;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve = {};
    colorAttachmentResolve.flags = 0;
    colorAttachmentResolve.format = surfaceFormat.format;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef = {};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth + stencil
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.flags = 0;
    depthAttachment.format = depthStencilFormat;
    depthAttachment.samples = sampleCounts;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
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

    constexpr uint32_t attachmentDescCount = 3;
    const VkAttachmentDescription attachment_descs[attachmentDescCount] = {
        color_attachment,
        depthAttachment,
        colorAttachmentResolve,
    };

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = attachmentDescCount;
    renderPassCreateInfo.pAttachments = &attachment_descs[0];
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass));
}

void Renderer::InitUniformBuffer()
{
    for (uint32_t i = 0; i < FramesInFlightType::MAX_FIF_COUNT; i++)
    {
        constexpr VkDeviceSize bufferSize = sizeof(PerFrameDataCpu);

        vk_create_buffer(device, gpu, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nullptr,
            &uniformBufferFrames.buffers[i], &uniformBufferFrames.memory[i]);
    }
}

void Renderer::InitPipeline()
{
    auto vertShaderCode = FileSystem::ReadFile(
        "../Resources/Shaders/vert.spv");
    auto fragShaderCode = FileSystem::ReadFile(
        "../Resources/Shaders/frag.spv");

    VkShaderModule shaderModules[2] = {};

    VkShaderModuleCreateInfo vertModuleInfo = {};
    vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleInfo.flags = 0;
    vertModuleInfo.codeSize = static_cast<uint32_t>(vertShaderCode.size());
    vertModuleInfo.pCode = reinterpret_cast<const uint32_t *>(vertShaderCode.data());

    VK_CHECK(
        vkCreateShaderModule(
            device,
            &vertModuleInfo,
            nullptr,
            &shaderModules[0]));

    VkShaderModuleCreateInfo fragModuleInfo = {};
    fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleInfo.flags = 0;
    fragModuleInfo.codeSize = static_cast<uint32_t>(fragShaderCode.size());
    fragModuleInfo.pCode = reinterpret_cast<const uint32_t *>(fragShaderCode.data());

    VK_CHECK(vkCreateShaderModule(device, &fragModuleInfo, nullptr, &shaderModules[1]));

    VkPipelineShaderStageCreateInfo vertStageInfo = {};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.flags = 0;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = shaderModules[0];
    vertStageInfo.pName = "main";
    vertStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragStageInfo = {};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.flags = 0;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = shaderModules[1];
    fragStageInfo.pName = "main";
    fragStageInfo.pSpecializationInfo = nullptr;

    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertStageInfo,
        fragStageInfo
    };

    const std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.flags = 0;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    constexpr VkVertexInputBindingDescription bindDesc[] = {
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

    constexpr VkVertexInputAttributeDescription attributeDescs[3] = {
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

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 3;
    vertexInputInfo.pVertexBindingDescriptions = &bindDesc[0];
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDescs[0];

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
    inputAssemblyInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.pNext = nullptr;
    inputAssemblyInfo.flags = 0;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

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
    VkPipelineViewportStateCreateInfo viewportInfo = {};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.flags = 0;
    viewportInfo.viewportCount = 1;
    viewportInfo.pViewports = &viewport;
    viewportInfo.scissorCount = 1;
    viewportInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
    rasterizationInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.flags = 0;
    rasterizationInfo.depthClampEnable = VK_FALSE;
    rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationInfo.depthBiasEnable = VK_FALSE;
    rasterizationInfo.depthBiasConstantFactor = 0.0f;
    rasterizationInfo.depthBiasClamp = 0.0f;
    rasterizationInfo.depthBiasSlopeFactor = 0.0f;
    rasterizationInfo.lineWidth = 1.0f;

    VkSampleCountFlagBits sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(gpu, &sampleCounts);

    VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.flags = 0;
    multisampleInfo.rasterizationSamples = sampleCounts;
    multisampleInfo.sampleShadingEnable = VK_TRUE;
    multisampleInfo.minSampleShading = 1.0f;
    multisampleInfo.pSampleMask = nullptr;
    multisampleInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.flags = 0;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;
    colorBlendInfo.blendConstants[0] = 0.0f;
    colorBlendInfo.blendConstants[1] = 0.0f;
    colorBlendInfo.blendConstants[2] = 0.0f;
    colorBlendInfo.blendConstants[3] = 0.0f;

    VkDescriptorSetLayoutBinding uniformBufferSetBinding = {};
    uniformBufferSetBinding.binding = 0;
    uniformBufferSetBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBufferSetBinding.descriptorCount = 1;
    uniformBufferSetBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uniformBufferSetBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uniformBufferSetBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
        &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.flags = 0;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
        &pipelineLayout));

    // depth + stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
    depthStencilStateCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.front = {};
    depthStencilStateCreateInfo.back = {};
    depthStencilStateCreateInfo.minDepthBounds = 0.0f;
    depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pTessellationState = nullptr;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterizationInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkPipelineRasterizationStateCreateInfo rasterizationInfoWireframe = {};
    rasterizationInfoWireframe.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfoWireframe.flags = 0;
    rasterizationInfoWireframe.depthClampEnable = VK_FALSE;
    rasterizationInfoWireframe.rasterizerDiscardEnable = VK_FALSE;
    rasterizationInfoWireframe.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizationInfoWireframe.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationInfoWireframe.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationInfoWireframe.depthBiasEnable = VK_FALSE;
    rasterizationInfoWireframe.depthBiasConstantFactor = 0.0f;
    rasterizationInfoWireframe.depthBiasClamp = 0.0f;
    rasterizationInfoWireframe.depthBiasSlopeFactor = 0.0f;
    rasterizationInfoWireframe.lineWidth = 1.0f;

    VkGraphicsPipelineCreateInfo pipelineInfoWireframe = {};
    pipelineInfoWireframe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfoWireframe.flags = 0;
    pipelineInfoWireframe.stageCount = 2;
    pipelineInfoWireframe.pStages = shaderStages;
    pipelineInfoWireframe.pVertexInputState = &vertexInputInfo;
    pipelineInfoWireframe.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfoWireframe.pTessellationState = nullptr;
    pipelineInfoWireframe.pViewportState = &viewportInfo;
    pipelineInfoWireframe.pRasterizationState = &rasterizationInfoWireframe;
    pipelineInfoWireframe.pMultisampleState = &multisampleInfo;
    pipelineInfoWireframe.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineInfoWireframe.pColorBlendState = &colorBlendInfo;
    pipelineInfoWireframe.pDynamicState = &dynamicStateCreateInfo;
    pipelineInfoWireframe.layout = pipelineLayout;
    pipelineInfoWireframe.renderPass = renderPass;
    pipelineInfoWireframe.subpass = 0;
    pipelineInfoWireframe.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfoWireframe.basePipelineIndex = -1;

    VkGraphicsPipelineCreateInfo pipeline_infos[] = {pipelineInfo,
                                                     pipelineInfoWireframe};

    VkPipeline pipelines[] = {pipeline, pipelineWireframe};

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 2, &pipeline_infos[0],
        nullptr, &pipelines[0]));

    pipeline = pipelines[0];
    pipelineWireframe = pipelines[1];

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = FramesInFlightType::MAX_FIF_COUNT;

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = FramesInFlightType::MAX_FIF_COUNT;
    poolCreateInfo.poolSizeCount = 1;
    poolCreateInfo.pPoolSizes = &poolSize;

    VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr,
        &descriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(FramesInFlightType::MAX_FIF_COUNT,
        descriptorSetLayout);

    VkDescriptorSetAllocateInfo setAllocateInfo = {};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = descriptorPool;
    setAllocateInfo.descriptorSetCount = FramesInFlightType::MAX_FIF_COUNT;
    setAllocateInfo.pSetLayouts = &layouts[0];

    VK_CHECK(vkAllocateDescriptorSets(device, &setAllocateInfo,
        &uniformBufferFrames.descriptorSets[0]));

    for (size_t i = 0; i < FramesInFlightType::MAX_FIF_COUNT; i++)
    {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBufferFrames.buffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(PerFrameDataCpu);

        VkWriteDescriptorSet descriptorSet = {};
        descriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSet.dstSet = uniformBufferFrames.descriptorSets[i];
        descriptorSet.dstBinding = 0;
        descriptorSet.dstArrayElement = 0;
        descriptorSet.descriptorCount = 1;
        descriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSet.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorSet, 0, nullptr);
    }

    vkDestroyShaderModule(device, shaderModules[0], nullptr);
    vkDestroyShaderModule(device, shaderModules[1], nullptr);
}

void Renderer::PrepareDepthStencil()
{
    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(framesInFlight.commandBuffer[0], &commandBufferBeginInfo);

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
