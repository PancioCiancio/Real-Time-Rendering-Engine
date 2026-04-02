//
// Created by apant on 02/08/2025.
//

#include "Renderer.h"

#include "../FileSystem.h"
#include "MeshLoader.h"
#include "VkCommon.h"
#include "Memory.h"

#include "vk_core_utils.h"
#include "vk_instance_utils.h"
#include "vk_physical_device_utils.h"
#include "vk_device_utils.h"
#include "vk_queue_utils.h"
#include "vk_surface_utils.h"
#include "vk_swapchain_utils.h"
#include "vk_memory_utils.h"

#include <SDL2/SDL_vulkan.h>

#include "vk_memory_utils.h"

namespace Utils
{

float ToClosestPowerOfTwo(const float x)
{
    return pow(2.0f, ceil(log2(x)));
}

}

namespace Renderer
{
// @todo just for testing purpose.
static uint32_t INDICES_COUNT = {};
static VkDeviceSize positionOffset = {};
static VkDeviceSize normalOffset = {};
static VkDeviceSize colorOffset = {};

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
    void *pUserData)
{
    // ReSharper disable once CppDFAUnusedValue
    const char *severity;
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
    vk_utils::VK_CHECK((SDL_Init(SDL_INIT_VIDEO) == 0)
        ? VK_SUCCESS
        : VK_ERROR_UNKNOWN);

    window = SDL_CreateWindow(
        "Adro Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640,
        480,
        SDL_WINDOW_VULKAN);

    vk_utils::VK_CHECK(volkInitialize());

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

        vkWaitForFences(device_, 1, &framesInFlight.submitFence[fifIndex],
            VK_TRUE, UINT64_MAX);

        uint32_t next_image = 0u;
        vk_utils::VK_CHECK(vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
            framesInFlight.acquiredImageSemaphore[fifIndex],
            VK_NULL_HANDLE, &next_image));

        vkResetFences(device_, 1, &framesInFlight.submitFence[fifIndex]);
        vkResetCommandPool(device_, framesInFlight.commandPool[fifIndex], 0);

        PerFrameDataCpu uBuffer = {
            glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp),
            glm::perspectiveRH_ZO(glm::radians(45.0f),
                static_cast<float>(extent_.width) /
                static_cast<float>(extent_.height),
                0.1f, 10000.0f),
        };

        // Flip vulkan Y-axis
        uBuffer.projection[1][1] *= -1;

        vk_utils::VK_CHECK(vkMapMemory(device_, uniformBufferFrames.memory[fifIndex], 0,
            sizeof(uBuffer), 0, &uniformBufferFrames.data_mapped[fifIndex]));

        constexpr size_t uBufferSize = sizeof(PerFrameDataCpu);

        memcpy(uniformBufferFrames.data_mapped[fifIndex], &uBuffer, uBufferSize);

        vkUnmapMemory(device_, uniformBufferFrames.memory[fifIndex]);

        VkCommandBufferBeginInfo commandBufferBeginInfo = {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;

        vk_utils::VK_CHECK(vkBeginCommandBuffer(framesInFlight.commandBuffer[fifIndex],
            &commandBufferBeginInfo));


        // Buffer copy cmd barrier
        VkBufferMemoryBarrier memoryBarrier[2] = {};
        // From read to write
        memoryBarrier[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier[0].size = vertexBufferSize;
        memoryBarrier[0].buffer = vertexBuffer;
        memoryBarrier[0].srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        memoryBarrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier[0].offset = 0;
        memoryBarrier[0].srcQueueFamilyIndex = present_queue_family_index_;
        memoryBarrier[0].dstQueueFamilyIndex = present_queue_family_index_;
        vkCmdPipelineBarrier(framesInFlight.commandBuffer[fifIndex],
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 1, &memoryBarrier[0], 0, nullptr);

        // Buffer Copy cmd
        VkBufferCopy bufferCopy = {};
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(framesInFlight.commandBuffer[fifIndex],
            stageVertexBuffer,
            vertexBuffer,
            1,
            &bufferCopy);

        // From write to read
        memoryBarrier[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier[1].size = vertexBufferSize;
        memoryBarrier[1].buffer = vertexBuffer;
        memoryBarrier[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier[1].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        memoryBarrier[1].offset = 0;
        memoryBarrier[1].srcQueueFamilyIndex = present_queue_family_index_;
        memoryBarrier[1].dstQueueFamilyIndex = present_queue_family_index_;
        vkCmdPipelineBarrier(framesInFlight.commandBuffer[fifIndex],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0, 0, nullptr, 1, &memoryBarrier[1], 0, nullptr);

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
        renderPassBeginInfo.renderArea.extent = extent_;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = &CLEAR_VALUES[0];

        vkCmdBeginRenderPass(framesInFlight.commandBuffer[fifIndex],
            &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(framesInFlight.commandBuffer[fifIndex],
            VK_PIPELINE_BIND_POINT_GRAPHICS, chosenPipeline);

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent_.width);
        viewport.height = static_cast<float>(extent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(framesInFlight.commandBuffer[fifIndex], 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = extent_;

        vkCmdSetScissor(framesInFlight.commandBuffer[fifIndex], 0, 1, &scissor);

        const VkBuffer bindsBuffer[] = {
            vertexBuffer,
            vertexBuffer,
            vertexBuffer,
        };

        const VkDeviceSize OFFSETS[] = {
            positionOffset,
            normalOffset,
            colorOffset
        };

        vkCmdBindVertexBuffers(framesInFlight.commandBuffer[fifIndex], 0, 3, &bindsBuffer[0],
            &OFFSETS[0]);

        vkCmdBindIndexBuffer(framesInFlight.commandBuffer[fifIndex],
            batch.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(framesInFlight.commandBuffer[fifIndex], INDICES_COUNT,
            1, 0, 0, 0);

        vkCmdEndRenderPass(framesInFlight.commandBuffer[fifIndex]);

        vk_utils::VK_CHECK(vkEndCommandBuffer(framesInFlight.commandBuffer[fifIndex]));

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

        vk_utils::VK_CHECK(vkQueueSubmit(present_queue_, 1, &submitInfo, framesInFlight.submitFence[fifIndex]));

        VkResult result = {};
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &presentationFrames.renderFinishedSemaphore[next_image];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_;
        presentInfo.pImageIndices = &next_image;
        presentInfo.pResults = &result;

        // @todo: cannot present the image if the window is minimized.
        vk_utils::VK_CHECK(vkQueuePresentKHR(present_queue_, &presentInfo));

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
    vk_utils::VK_CHECK(
        vkDeviceWaitIdle(device_));

    vkDestroyDescriptorPool(
        device_,
        descriptorPool,
        nullptr);
    vkDestroyDescriptorSetLayout(
        device_,
        descriptorSetLayout,
        nullptr);
    vkDestroyRenderPass(
        device_,
        renderPass,
        nullptr);
    vkDestroyPipelineLayout(
        device_,
        pipelineLayout,
        nullptr);
    vkDestroyPipeline(
        device_,
        pipeline,
        nullptr);
    vkDestroyPipeline(
        device_,
        pipelineWireframe,
        nullptr);
    vkDestroyBuffer(
        device_,
        stageVertexBuffer,
        nullptr);
    vkFreeMemory(
        device_,
        stageVertexMemory,
        nullptr);
    vkDestroyBuffer(
        device_,
        vertexBuffer,
        nullptr);
    vkFreeMemory(
        device_,
        vertexMemory,
        nullptr);
    vkDestroyBuffer(
        device_,
        batch.indexBuffer,
        nullptr);
    vkFreeMemory(
        device_,
        batch.indexMem,
        nullptr);

    for (const VkDeviceMemory &memory : uniformBufferFrames.memory)
    {
        vkFreeMemory(
            device_,
            memory,
            nullptr);
    }

    for (const VkBuffer &buffer : uniformBufferFrames.buffers)
    {
        vkDestroyBuffer(
            device_,
            buffer,
            nullptr);
    }

    // Multisaple image
    vkDestroyImageView(
        device_,
        framebufferSampleImageView,
        nullptr);
    vkDestroyImage(
        device_,
        framebufferSampleImage,
        nullptr);
    vkFreeMemory(
        device_,
        framebufferSampleImageMemory,
        nullptr);

    // Depth stencil
    vkDestroyImageView(
        device_,
        depthStencilImageView,
        nullptr);
    vkDestroyImage(
        device_,
        depthStencilImage,
        nullptr);
    vkFreeMemory(
        device_,
        depthStencilMemory,
        nullptr);

    for (uint32_t i = 0; i < image_count_; i++)
    {
        vkDestroyFramebuffer(
            device_,
            presentationFrames.framebuffer[i],
            nullptr);

        vkDestroyImageView(
            device_,
            presentationFrames.imageView[i],
            nullptr);

        vkDestroySemaphore(
            device_,
            presentationFrames.renderFinishedSemaphore[i],
            nullptr);
    }

    for (const VkSemaphore &framesInFlightSemaphore : framesInFlight.acquiredImageSemaphore)
    {
        vkDestroySemaphore(
            device_,
            framesInFlightSemaphore,
            nullptr);
    }

    for (const VkFence &framesInFlightFence : framesInFlight.submitFence)
    {
        vkDestroyFence(
            device_,
            framesInFlightFence,
            nullptr);
    }

    vkDestroySwapchainKHR(
        device_,
        swapchain_,
        nullptr);

    for (const VkCommandPool &framesInFlightCommandPool : framesInFlight.commandPool)
    {
        vkDestroyCommandPool(
            device_,
            framesInFlightCommandPool,
            nullptr);
    }

    vkDestroyDevice(
        device_,
        nullptr);
    vkDestroySurfaceKHR(
        instance_,
        surface_,
        nullptr);
    vkDestroyDebugUtilsMessengerEXT(
        instance_,
        debug_messenger_,
        nullptr);
    vkDestroyInstance(
        instance_,
        nullptr);

    SDL_DestroyWindow(
        window);
    SDL_Quit();
}

void Renderer::InitInstance()
{
    vk_utils::VK_CHECK(volkInitialize());

    // @todo:	calculate the layers count from the array.
    const char *requestedLayers[] = {"VK_LAYER_KHRONOS_validation"};

    // @todo:	calculate the extension count from the array.
    const char *requestedExtensions[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

    instance_ = vk_utils::CreateInstance(requestedLayers, requestedExtensions).value();

    volkLoadInstance(instance_);

    vkCreateDebugUtilsMessengerEXT(
        instance_,
        &DEBUG_UTILS_MESSENGER_CREATE_INFO,
        nullptr,
        &debug_messenger_);
}

void Renderer::InitSurface()
{
    // @todo use the vulkan call and not the sdl one.
    vk_utils::VK_CHECK(
        SDL_Vulkan_CreateSurface(
            window,
            instance_,
            &surface_)
        ? VK_SUCCESS
        : VK_ERROR_INITIALIZATION_FAILED);
}

void Renderer::InitDevice()
{
    VkPhysicalDeviceFeatures required_features = {};
    required_features.geometryShader = VK_TRUE;
    required_features.tessellationShader = VK_TRUE;
    required_features.multiDrawIndirect = VK_TRUE;
    required_features.fillModeNonSolid = VK_TRUE;
    required_features.sampleRateShading = VK_TRUE;
    required_features.samplerAnisotropy = VK_TRUE;

    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    physical_device_ = vk_utils::CreatePhysicalDevice(instance_, device_extensions, required_features).value();

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device_,
        &queue_family_count,
        nullptr);    // pQueueFamilyProperties;

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device_,
        &queue_family_count,
        queue_family_properties.data());

    device_ = vk_utils::CreateDevice(physical_device_, device_extensions, required_features).value();

    volkLoadDevice(device_);

    present_queue_family_index_ = vk_utils::GetPresentQueueIndex(physical_device_, surface_, queue_family_properties).value();
    vkGetDeviceQueue(device_, present_queue_family_index_, 0, &present_queue_);
}

void Renderer::InitSwapchain()
{
    surfaceFormat = vk_utils::SelectSurfaceFormat(
        physical_device_,
        surface_,
        {VK_FORMAT_R8G8B8A8_SRGB, std::nullopt}).value();

    vk_utils::SwapchainResult swapchain_result = vk_utils::CreateSwapchain(
        device_,
        physical_device_,
        surface_,
        {
            surfaceFormat,
            {640, 480},
            std::nullopt,
            VK_NULL_HANDLE
        }).value();

    swapchain_ = swapchain_result.swapchain;
    extent_ = swapchain_result.extent;
    image_count_ = swapchain_result.image_count;

    presentationFrames.image = vk_utils::GetSwapchainImages(device_, swapchain_).value();
    presentationFrames.imageView.reserve(image_count_);
    presentationFrames.framebuffer.reserve(image_count_);
    presentationFrames.renderFinishedSemaphore.reserve(image_count_);

    // Create the view of the swapchain images
    for (uint32_t i = 0; i < image_count_; i++)
    {
        VkImageView image_view = {};
        image_view = vk_utils::CreateImageView(
            device_,
            presentationFrames.image[i],
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            surfaceFormat.format,
            {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            }).value();
        presentationFrames.imageView.push_back(image_view);
    }

    for (uint32_t i = 0; i < image_count_; i++)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.flags = 0;

        VkSemaphore semaphore = {};
        vk_utils::VK_CHECK(
            vkCreateSemaphore(
                device_,
                &semaphoreCreateInfo,
                nullptr,
                &semaphore));
        presentationFrames.renderFinishedSemaphore.push_back(semaphore);
    }
}

void Renderer::InitOtherImages()
{
    VkSampleCountFlagBits sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(
        physical_device_,
        &sampleCounts);

    vk_utils::ImageResult sample_image_result = vk_utils::CreateImage(
        device_,
        physical_device_,
        VK_IMAGE_TYPE_2D,
        surfaceFormat.format,
        {
            extent_.width,
            extent_.height,
            1
        },
        sampleCounts,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).value();

    framebufferSampleImage = sample_image_result.image;
    framebufferSampleImageMemory = sample_image_result.memory;

    framebufferSampleImageView = vk_utils::CreateImageView(
        device_,
        framebufferSampleImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        surfaceFormat.format,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        }).value();

    // Depth + Stencil

    constexpr VkFormat depthStencilFormatsRequested[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };

    vk_query_supported_format(
        physical_device_,
        4,
        &depthStencilFormatsRequested[0],
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        &depthStencilFormat);

    vk_utils::ImageResult depth_stencil_image_result = vk_utils::CreateImage(
        device_,
        physical_device_,
        VK_IMAGE_TYPE_2D,
        depthStencilFormat,
        {extent_.width, extent_.height, 1},
        sampleCounts,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).value();

    depthStencilImage = depth_stencil_image_result.image;
    depthStencilMemory = depth_stencil_image_result.memory;

     depthStencilImageView = vk_utils::CreateImageView(
        device_,
        depthStencilImage,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        depthStencilFormat,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        }).value();
}

void Renderer::InitCommand()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                  VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = present_queue_family_index_;

    for (size_t i = 0; i < FramesInFlightType::MAX_FIF_COUNT; i++)
    {
        VkCommandPool &fifCommandPool = framesInFlight.commandPool[i];

        vk_utils::VK_CHECK(vkCreateCommandPool(device_, &commandPoolCreateInfo, nullptr,
            &fifCommandPool));

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = fifCommandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        vk_utils::VK_CHECK(vkAllocateCommandBuffers(device_, &commandBufferAllocateInfo,
            &framesInFlight.commandBuffer[i]));

        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.flags = 0;

        vk_utils::VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr,
            &framesInFlight.acquiredImageSemaphore[i]));

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vk_utils::VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr,
            &framesInFlight.submitFence[i]));
    }
}


void Renderer::InitBatch()
{
    // Get the gpu required memory alignment
    VkPhysicalDeviceProperties physicalDeviceProperties = {};
    vkGetPhysicalDeviceProperties(physical_device_, &physicalDeviceProperties);
    const VkDeviceSize minAlignment = physicalDeviceProperties.limits.minMemoryMapAlignment;

    // Load the mesh.
    MeshLoader::Load("../resources/meshes/SM_Behemoth.fbx", &batchData);

    // Update the global indices count.
    INDICES_COUNT = batchData.indices.size();

    // Update the colors data.
    std::vector<glm::vec4> defaultColors(batchData.position.size(),
        glm::vec4(.36f, .36f, .6391f, 1.0f));
    batchData.color = defaultColors;

    // Raw data
    const VkDeviceSize positionSize = sizeof(glm::vec3) * batchData.position.size();
    const VkDeviceSize normalSize = sizeof(glm::vec3) * batchData.normals.size();
    const VkDeviceSize colorSize = sizeof(glm::vec4) * batchData.color.size();

    // Calculate aligned offset
    positionOffset = 0;
    normalOffset = Memory::Align(positionOffset + positionSize, minAlignment);
    colorOffset = Memory::Align(normalOffset + normalSize, minAlignment);

    // The total size required for the buffer
    vertexBufferSize = colorOffset + colorSize;

    // Create the staging buffer
    vk_create_buffer(device_,
        physical_device_,
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &stageVertexBuffer,
        &stageVertexMemory);

    // Create the device local buffer
    vk_create_buffer(device_,
        physical_device_,
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        nullptr,
        &vertexBuffer,
        &vertexMemory);

    void *data = {};

    // Map the entire buffer's memory range.
    vkMapMemory(device_, stageVertexMemory, 0, vertexBufferSize, 0, &data);

    // Copy each data set to its calculated offset
    char *basePtr = static_cast<char *>(data);
    memcpy(basePtr + positionOffset, batchData.position.data(), positionSize);
    memcpy(basePtr + normalOffset, batchData.normals.data(), normalSize);
    memcpy(basePtr + colorOffset, batchData.color.data(), colorSize);

    // Unmap the memory
    vkUnmapMemory(device_, stageVertexMemory);

    const size_t actualIndexBufferSize = sizeof(uint32_t) * batchData.indices.size();
    const size_t indexBufferSize = static_cast<size_t>(Utils::ToClosestPowerOfTwo(
        static_cast<float>(actualIndexBufferSize)));
    vk_create_buffer(device_, physical_device_, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        nullptr,
        &batch.indexBuffer, &batch.indexMem);

    void *index_data = nullptr;
    vk_utils::VK_CHECK(
        vkMapMemory(
            device_,
            batch.indexMem,
            0,
            indexBufferSize,
            0,
            &index_data));

    memcpy(
        index_data,
        &batchData.indices[0],
        sizeof(uint32_t) * batchData.indices.size());

    vkUnmapMemory(
        device_,
        batch.indexMem);
}

void Renderer::InitFramebuffers()
{
    for (size_t i = 0; i < image_count_; i++)
    {
        const VkImageView attachments[3] = {
            framebufferSampleImageView, // Multisample
            depthStencilImageView,
            presentationFrames.imageView[i], // Multisample resolver to 1 sample.
        };

        VkFramebufferCreateInfo framebuffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = renderPass,
            .attachmentCount = 3,
            .pAttachments = &attachments[0],
            .width = extent_.width,
            .height = extent_.height,
            .layers = 1,
        };

        VkFramebuffer framebuffer = {};
        vk_utils::VK_CHECK(
            vkCreateFramebuffer(
                device_,
                &framebuffer_create_info,
                nullptr,
                &framebuffer));
        presentationFrames.framebuffer.push_back(framebuffer);
    }
}

void Renderer::InitRenderpass()
{
    VkSampleCountFlagBits sample_counts = VK_SAMPLE_COUNT_1_BIT;
    vk_query_sample_counts(
        physical_device_,
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

    VkAttachmentDescription color_resolver_attachment = {};
    color_resolver_attachment.flags = 0;
    color_resolver_attachment.format = surfaceFormat.format;
    color_resolver_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_resolver_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_resolver_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_resolver_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_resolver_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_resolver_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_resolver_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_resolver_attachment_ref = {};
    color_resolver_attachment_ref.attachment = 2;
    color_resolver_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pResolveAttachments = &color_resolver_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;
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

    constexpr uint32_t attachemnt_desc_count = 3;
    const VkAttachmentDescription attachment_descs[attachemnt_desc_count] = {
        color_attachment,
        depth_attachment,
        color_resolver_attachment,
    };

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.pNext = nullptr;
    render_pass_create_info.flags = 0;
    render_pass_create_info.attachmentCount = attachemnt_desc_count;
    render_pass_create_info.pAttachments = &attachment_descs[0];
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    vk_utils::VK_CHECK(vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &renderPass));
}

void Renderer::InitUniformBuffer()
{
    for (uint32_t i = 0; i < FramesInFlightType::MAX_FIF_COUNT; i++)
    {
        constexpr VkDeviceSize bufferSize = sizeof(PerFrameDataCpu);

        vk_create_buffer(device_, physical_device_, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nullptr,
            &uniformBufferFrames.buffers[i], &uniformBufferFrames.memory[i]);
    }
}

void Renderer::InitPipeline()
{
    auto vertShaderCode = FileSystem::ReadFile(
        "../resources/shaders/vert.spv");
    auto fragShaderCode = FileSystem::ReadFile(
        "../resources/shaders/frag.spv");

    VkShaderModule shaderModules[2] = {};

    VkShaderModuleCreateInfo vertModuleInfo = {};
    vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleInfo.flags = 0;
    vertModuleInfo.codeSize = static_cast<uint32_t>(vertShaderCode.size());
    vertModuleInfo.pCode = reinterpret_cast<const uint32_t *>(vertShaderCode.data());

    vk_utils::VK_CHECK(
        vkCreateShaderModule(
            device_,
            &vertModuleInfo,
            nullptr,
            &shaderModules[0]));

    VkShaderModuleCreateInfo fragModuleInfo = {};
    fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleInfo.flags = 0;
    fragModuleInfo.codeSize = static_cast<uint32_t>(fragShaderCode.size());
    fragModuleInfo.pCode = reinterpret_cast<const uint32_t *>(fragShaderCode.data());

    vk_utils::VK_CHECK(vkCreateShaderModule(device_, &fragModuleInfo, nullptr, &shaderModules[1]));

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
        // normal.
        {
            .binding = 1,
            .stride = sizeof(glm::vec3),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        },
        // color.
        {
            .binding = 2,
            .stride = sizeof(glm::vec4),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        },
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
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        },
        {
            .location = 2,
            .binding = 2,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0
        },

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
        .width = static_cast<float>(extent_.width),
        .height = static_cast<float>(extent_.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D scissor = {
        .offset = {0, 0},
        .extent = extent_,
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
    vk_query_sample_counts(physical_device_, &sampleCounts);

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

    vk_utils::VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr,
        &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.flags = 0;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    vk_utils::VK_CHECK(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr,
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

    vk_utils::VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 2, &pipeline_infos[0],
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

    vk_utils::VK_CHECK(vkCreateDescriptorPool(device_, &poolCreateInfo, nullptr,
        &descriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(FramesInFlightType::MAX_FIF_COUNT,
        descriptorSetLayout);

    VkDescriptorSetAllocateInfo setAllocateInfo = {};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = descriptorPool;
    setAllocateInfo.descriptorSetCount = FramesInFlightType::MAX_FIF_COUNT;
    setAllocateInfo.pSetLayouts = &layouts[0];

    vk_utils::VK_CHECK(vkAllocateDescriptorSets(device_, &setAllocateInfo,
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

        vkUpdateDescriptorSets(device_, 1, &descriptorSet, 0, nullptr);
    }

    vkDestroyShaderModule(device_, shaderModules[0], nullptr);
    vkDestroyShaderModule(device_, shaderModules[1], nullptr);
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

    vkQueueSubmit(present_queue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(present_queue_);
}
} // Renderer