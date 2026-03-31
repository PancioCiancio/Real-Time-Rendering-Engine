#ifndef ORDA_GRAPHICS_ASSERT_H_
#define ORDA_GRAPHICS_ASSERT_H_

#include <Volk/volk.h>
#include <cassert>

/// Wrap most common vulkan calls (e.g. VkCreateInstance, VkCreateBuffer, VkCreateImage, ...).
/// To delete vulkan resources, you must do the normal vulkan calls (e.g. vkDestroyInstance, ...).

// @TODO: I would like something that can report the error. How should it be done?
#define VK_CHECK(result)					\
	do {                                    \
		VkResult _vk_result = (result);     \
		assert(_vk_result == VK_SUCCESS);	\
	} while (0)

#endif // ORDA_GRAPHICS_ASSERT_H_