#ifndef PARALLEL_RDP_H
#define PARALLEL_RDP_H

#include "m64p_plugin.h"

namespace RDP
{
bool init(GFX_INFO gfx_info);
void deinit();
void begin_frame();

void process_commands(GFX_INFO gfx_info);

void complete_frame();
void deinit();
}

#endif