/*
 * Copyright © 2024 Imagination Technologies Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PVR_ANDROID_H
#define PVR_ANDROID_H

#include <stdbool.h>
#include "vulkan/vulkan.h"

struct pvr_device;

#ifdef VK_USE_PLATFORM_ANDROID_KHR

bool pvr_android_is_gralloc_image(const VkImageCreateInfo *pCreateInfo);

VkResult pvr_android_create_gralloc_image(
   VkDevice device, const VkImageCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator, VkImage *pImage);

bool pvr_android_is_ahb_memory(const VkMemoryAllocateInfo *pAllocateInfo);

VkResult pvr_android_allocate_ahb_memory(
   VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
   const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory);

#else /* VK_USE_PLATFORM_ANDROID_KHR */

static inline bool
pvr_android_is_gralloc_image(const VkImageCreateInfo *pCreateInfo)
{
   return false;
}

static inline VkResult
pvr_android_create_gralloc_image(VkDevice device,
                                  const VkImageCreateInfo *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator,
                                  VkImage *pImage)
{
   return VK_ERROR_FEATURE_NOT_PRESENT;
}

static inline bool
pvr_android_is_ahb_memory(const VkMemoryAllocateInfo *pAllocateInfo)
{
   return false;
}

static inline VkResult
pvr_android_allocate_ahb_memory(VkDevice device,
                                 const VkMemoryAllocateInfo *pAllocateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkDeviceMemory *pMemory)
{
   return VK_ERROR_FEATURE_NOT_PRESENT;
}

#endif /* VK_USE_PLATFORM_ANDROID_KHR */

#endif /* PVR_ANDROID_H */
