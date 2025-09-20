//
// Created by apant on 02/08/2025.
//

#ifndef COMMON_H
#define COMMON_H

#include <Volk/volk.h>
#include <cstdint>

namespace Renderer {
	/// Wrap most common vulkan calls (e.g. VkCreateInstance, VkCreateBuffer, VkCreateImage, ...).
	/// To delete vulkan resources, you must do the normal vulkan calls (e.g. vkDestroyInstance, ...).

	// @TODO: I would like something that can report the error. How should it be done?
#define VK_CHECK(result)					\
	do {                                    \
		VkResult _vk_result = (result);     \
		assert(_vk_result == VK_SUCCESS);	\
	} while (0)


	// ==========================
	// Physical Device
	// ==========================

#pragma region VkPhysicalDevice
	void QueryGpu(
		VkInstance instance,
		VkPhysicalDeviceFeatures fts_requested,
		uint32_t requested_extension_count,
		const char **p_requested_extensions,
		VkPhysicalDevice *p_gpu);

	void vk_query_sample_counts(
		VkPhysicalDevice gpu,
		VkSampleCountFlagBits *p_sample);

	/// @todo Platform dependent. Consider to solve the platform implementation at static link time.
	void QueryQueueFamily(
		VkPhysicalDevice gpu,
		VkQueueFlagBits queue_flag_bits_requested,
		bool must_support_presentation,
		uint32_t family_idx_discarded_count,
		const uint32_t *family_idx_discarded,
		uint32_t *p_queue_family_idx);

	void vk_query_supported_format(
		VkPhysicalDevice gpu,
		uint32_t requested_format_count,
		const VkFormat *p_requested_formats,
		VkImageTiling tiling,
		VkFormatFeatureFlags features,
		VkFormat *p_format);

	void vk_query_surface_format(
		VkPhysicalDevice gpu,
		VkSurfaceKHR surface,
		uint32_t required_format_count,
		const VkFormat *p_required_formats,
		VkSurfaceFormatKHR *p_format);


	void vk_query_surface_capabilities(
		VkPhysicalDevice gpu,
		VkSurfaceKHR surface,
		VkSurfaceCapabilitiesKHR *p_surface_capabilities);


	/// @todo Sketch implementation. Refactor it...
	/// Go through every memory type to look what fit the request.
	uint32_t vk_query_memory_type_idx(
		uint32_t type_filter,
		VkMemoryPropertyFlags memory_property_flags,
		const VkPhysicalDeviceMemoryProperties &physical_device_memory_properties);
#pragma endregion


	// ==========================
	// Device
	// ==========================

#pragma region VkDevice
	void vk_create_device(
		VkPhysicalDevice gpu,
		uint32_t queue_family_count,
		const uint32_t *p_queue_families_idx,
		uint32_t requested_extension_count,
		const char **p_requested_extensions,
		const VkPhysicalDeviceFeatures *p_features,
		const VkAllocationCallbacks *p_allocator,
		VkDevice *p_device);
#pragma endregion


	// ==========================
	// Swapchain
	// ==========================

#pragma region VkSwapchainKHR
	void vk_create_swapchain(
		VkDevice device,
		VkSurfaceKHR surface,
		VkSurfaceFormatKHR *p_format,
		VkSurfaceCapabilitiesKHR *p_capabilities,
		VkSwapchainKHR old_swapchain,
		VkAllocationCallbacks *p_allocator,
		VkSwapchainKHR *p_swapchain);

	void vk_query_swapchain_images(
		VkDevice device,
		VkSwapchainKHR swapchain,
		VkImage *p_swapchain_images);
#pragma endregion


	// ==========================
	// Buffer
	// ==========================

#pragma region VkBuffer / VkBufferView
	void vk_create_buffer(
		VkDevice device,
		VkPhysicalDevice physical_device,
		VkDeviceSize size,
		VkBufferUsageFlags usage_flags,
		VkMemoryPropertyFlags memory_property_flag_bits,
		VkAllocationCallbacks *p_allocator,
		VkBuffer *p_buffer,
		VkDeviceMemory *p_memory);

	/// @warning	Provided buffer must be valid.
	void vk_create_buffer_view(
		VkDevice device,
		VkBuffer buffer,
		VkFormat format,
		VkDeviceSize offset,
		VkDeviceSize range,
		VkAllocationCallbacks *p_allocator,
		VkBufferView *p_buffer_view);
#pragma endregion


	// ==========================
	// Image
	// ==========================

#pragma region VkImage / VkImageView
	void vk_create_image(
		VkDevice device,
		VkPhysicalDevice physical_device,
		VkImageType image_type,
		VkFormat format,
		VkExtent3D extent,
		VkSampleCountFlagBits samples,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlagBits memory_property_flag_bits,
		VkAllocationCallbacks *p_allocator,
		VkImage *p_image,
		VkDeviceMemory *p_memory);

	/// @warning	Provided image must be valid.
	void vk_create_image_view(
		VkDevice device,
		VkImage image,
		VkImageAspectFlags aspect_flags,
		VkImageViewType view_type,
		VkFormat format,
		VkComponentMapping components,
		VkAllocationCallbacks *p_allocator,
		VkImageView *p_image_view);
#pragma endregion
}

#endif //COMMON_H
