#ifndef PARALLEL_RDP_H
#define PARALLEL_RDP_H

#include "m64p_plugin.h"
#include "m64p_vidext.h"
#include <vulkan/vulkan.h>

extern ptr_VidVkExtGetSyncIndex                      CoreVkVideo_GetSyncIndex;
extern ptr_VidVkExtGetSyncIndexMask                  CoreVkVideo_GetSyncIndexMask;
extern ptr_VidVkExtSetImage                          CoreVkVideo_SetImage;

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
                            VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr);

#endif
