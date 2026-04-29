/*
 * Copyright © 2024 Imagination Technologies Ltd.
 * SPDX-License-Identifier: MIT
 */

/* Android-specific Vulkan support for the PowerVR Mesa driver.
 *
 * Implements AHB (Android Hardware Buffer) memory allocation needed by
 * VK_ANDROID_external_memory_android_hardware_buffer.
 * ANB (Android Native Buffer / gralloc swapchain) handling lives in
 * pvr_image.c so that it can access the static image-init helpers.
 *
 * The common Mesa layer (src/vulkan/runtime/vk_android.c) provides:
 *  - vk_common_GetAndroidHardwareBufferPropertiesANDROID
 *  - vk_common_GetMemoryAndroidHardwareBufferANDROID
 *  - vk_common_GetSwapchainGrallocUsage{,2}ANDROID
 *  - vk_common_AcquireImageANDROID
 *  - vk_common_QueueSignalReleaseImageANDROID
 * These are registered automatically by vk_device_init() via
 * vk_common_device_entrypoints; no driver-side wrappers are needed.
 */

#ifdef VK_USE_PLATFORM_ANDROID_KHR

#include "pvr_android.h"
#include "pvr_device.h"
#include "pvr_entrypoints.h"

#include <vndk/hardware_buffer.h>

#include "drm-uapi/drm_fourcc.h"

#include "vk_android.h"
#include "vk_device_memory.h"
#include "vk_log.h"
#include "vk_util.h"

bool
pvr_android_is_gralloc_image(const VkImageCreateInfo *pCreateInfo)
{
   vk_foreach_struct_const(ext, pCreateInfo->pNext) {
      switch ((uint32_t)ext->sType) {
      case VK_STRUCTURE_TYPE_NATIVE_BUFFER_ANDROID:
         return true;
      case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO: {
         const VkExternalMemoryImageCreateInfo *external_info = (void *)ext;
         if (external_info->handleTypes &
             VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
            return true;
         break;
      }
      default:
         break;
      }
   }
   return false;
}

bool
pvr_android_is_ahb_memory(const VkMemoryAllocateInfo *pAllocateInfo)
{
   vk_foreach_struct_const(ext, pAllocateInfo->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
         return true;
      case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
         return ((const VkExportMemoryAllocateInfo *)ext)->handleTypes ==
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
      default:
         break;
      }
   }
   return false;
}

VkResult
pvr_android_create_gralloc_image(VkDevice device,
                                  const VkImageCreateInfo *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator,
                                  VkImage *pImage)
{
   /* AHB buffers are always linear (from gralloc).  Force
    * VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT + DRM_FORMAT_MOD_LINEAR so that
    * pvr_image_init_memlayout() picks PVR_MEMLAYOUT_LINEAR and computes the
    * correct image size.  Without this, OPTIMAL tiling would select
    * PVR_MEMLAYOUT_TWIDDLED, round extents up to the next power-of-two, and
    * produce an image->size larger than the AHB allocation, causing
    * pvr_bind_memory() / vma_map to fail.
    */
   const uint64_t linear_modifier = DRM_FORMAT_MOD_LINEAR;
   const VkImageDrmFormatModifierListCreateInfoEXT mod_list = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
      .pNext = pCreateInfo->pNext,
      .drmFormatModifierCount = 1,
      .pDrmFormatModifiers = &linear_modifier,
   };
   VkImageCreateInfo ahb_create_info = *pCreateInfo;
   ahb_create_info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
   ahb_create_info.pNext = &mod_list;

   return pvr_CreateImage(device, &ahb_create_info, pAllocator, pImage);
}

VkResult
pvr_android_allocate_ahb_memory(VkDevice device,
                                 const VkMemoryAllocateInfo *pAllocateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkDeviceMemory *pMemory)
{
   struct AHardwareBuffer *ahb;
   VkResult result;

   const VkImportAndroidHardwareBufferInfoANDROID *ahb_info =
      vk_find_struct_const(pAllocateInfo->pNext,
                           IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID);
   if (ahb_info) {
      ahb = ahb_info->buffer;
      AHardwareBuffer_acquire(ahb);
   } else {
      ahb = vk_alloc_ahardware_buffer(pAllocateInfo);
      if (!ahb) {
         VK_FROM_HANDLE(vk_device, dev, device);
         return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);
      }
   }

   /* Import the AHB as a dma-buf fd and delegate to the standard
    * pvr_AllocateMemory path.
    */
   const native_handle_t *handle = AHardwareBuffer_getNativeHandle(ahb);
   if (!handle || handle->numFds < 1) {
      AHardwareBuffer_release(ahb);
      VK_FROM_HANDLE(vk_device, dev, device);
      return vk_error(dev, VK_ERROR_INVALID_EXTERNAL_HANDLE);
   }

   int dup_fd = dup(handle->data[0]);
   if (dup_fd < 0) {
      AHardwareBuffer_release(ahb);
      VK_FROM_HANDLE(vk_device, dev, device);
      return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   const VkMemoryDedicatedAllocateInfo *dedicated_info =
      vk_find_struct_const(pAllocateInfo->pNext,
                           MEMORY_DEDICATED_ALLOCATE_INFO);

   const VkMemoryDedicatedAllocateInfo local_dedicated = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .image = dedicated_info ? dedicated_info->image : VK_NULL_HANDLE,
      .buffer = dedicated_info ? dedicated_info->buffer : VK_NULL_HANDLE,
   };
   const VkImportMemoryFdInfoKHR fd_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
      .pNext = &local_dedicated,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      .fd = dup_fd,
   };
   const VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &fd_info,
      .allocationSize = pAllocateInfo->allocationSize,
      .memoryTypeIndex = pAllocateInfo->memoryTypeIndex,
   };

   VK_FROM_HANDLE(vk_device, dev, device);
   result = dev->dispatch_table.AllocateMemory(device, &alloc_info, pAllocator,
                                               pMemory);
   if (result != VK_SUCCESS) {
      close(dup_fd);
      AHardwareBuffer_release(ahb);
      return result;
   }

   VK_FROM_HANDLE(vk_device_memory, mem, *pMemory);
   assert(!mem->ahardware_buffer);
   mem->ahardware_buffer = ahb;

   return VK_SUCCESS;
}

#endif /* VK_USE_PLATFORM_ANDROID_KHR */
