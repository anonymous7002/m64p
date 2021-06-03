#ifndef PARALLEL_RDP_H
#define PARALLEL_RDP_H

#include "libretro_vulkan.h"
#include "m64p_plugin.h"

namespace RDP
{
bool init(GFX_INFO gfx_info);
void deinit();
void begin_frame();

extern const struct retro_hw_render_interface_vulkan *vulkan;
void process_commands(GFX_INFO gfx_info);

void complete_frame(GFX_INFO gfx_info);
void deinit();
}

#endif
