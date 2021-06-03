#ifndef PARALLEL_RDP_H
#define PARALLEL_RDP_H

#include "m64p_plugin.h"
#include <vulkan/vulkan.h>

struct retro_vulkan_image
 {
  VkImageView image_view;
  VkImageLayout image_layout;
  VkImageViewCreateInfo create_info;
 };

namespace RDP
{
bool init(GFX_INFO gfx_info);
void deinit();
void begin_frame();

void process_commands(GFX_INFO gfx_info);

void complete_frame(GFX_INFO gfx_info);
void deinit();
}

bool parallel_create_device(VkInstance instance, VkPhysicalDevice gpu,
                            VkDevice device, VkQueue queue, uint32_t queue_family, PFN_vkGetInstanceProcAddr get_instance_proc_addr);

#endif
