/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-video-parallel - gfx_m64p.cpp                             *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
 *   Copyright (C) 2009 Richard Goedeken                                   *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define M64P_PLUGIN_PROTOTYPES 1

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gfx_m64p.h"
#include "rdp.h"

#include "m64p_vidext.h"
#include "m64p_types.h"
#include "m64p_config.h"

static ptr_ConfigOpenSection      ConfigOpenSection = NULL;
static ptr_ConfigSaveSection      ConfigSaveSection = NULL;
static ptr_ConfigSetDefaultInt    ConfigSetDefaultInt = NULL;
static ptr_ConfigSetDefaultBool   ConfigSetDefaultBool = NULL;
static ptr_ConfigGetParamInt      ConfigGetParamInt = NULL;
static ptr_ConfigGetParamBool     ConfigGetParamBool = NULL;
static ptr_PluginGetVersion       CoreGetVersion = NULL;
static ptr_VidVkExt_Init           CoreVkVideo_Init = NULL;

void (*debug_callback)(void *, int, const char *);
void *debug_call_context;

m64p_dynlib_handle CoreLibHandle;
GFX_INFO gfx_info;
void (*render_callback)(int);


#define PLUGIN_VERSION              0x000100
#define VIDEO_PLUGIN_API_VERSION    0x020200

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle _CoreLibHandle, void *Context,
                                     void (*DebugCallback)(void *, int, const char *))
{
    /* first thing is to set the callback function for debug info */
    debug_callback = DebugCallback;
    debug_call_context = Context;

    CoreLibHandle = _CoreLibHandle;

    ConfigOpenSection = (ptr_ConfigOpenSection)DLSYM(CoreLibHandle, "ConfigOpenSection");
    ConfigSaveSection = (ptr_ConfigSaveSection)DLSYM(CoreLibHandle, "ConfigSaveSection");
    ConfigSetDefaultInt = (ptr_ConfigSetDefaultInt)DLSYM(CoreLibHandle, "ConfigSetDefaultInt");
    ConfigSetDefaultBool = (ptr_ConfigSetDefaultBool)DLSYM(CoreLibHandle, "ConfigSetDefaultBool");
    ConfigGetParamInt = (ptr_ConfigGetParamInt)DLSYM(CoreLibHandle, "ConfigGetParamInt");
    ConfigGetParamBool = (ptr_ConfigGetParamBool)DLSYM(CoreLibHandle, "ConfigGetParamBool");

    CoreVkVideo_Init = (ptr_VidVkExt_Init) DLSYM(CoreLibHandle, "VidVkExt_Init");

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL PluginShutdown(void)
{
    /* reset some local variable */
    debug_callback = NULL;
    debug_call_context = NULL;

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion, int *APIVersion, const char **PluginNamePtr, int *Capabilities)
{
    /* set version info */
    if (PluginType != NULL) {
        *PluginType = M64PLUGIN_GFX;
    }

    if (PluginVersion != NULL) {
        *PluginVersion = PLUGIN_VERSION;
    }

    if (APIVersion != NULL) {
        *APIVersion = VIDEO_PLUGIN_API_VERSION;
    }

    if (PluginNamePtr != NULL) {
        *PluginNamePtr = "paraLLEl-RDP";
    }

    if (Capabilities != NULL) {
        *Capabilities = 0;
    }

    return M64ERR_SUCCESS;
}

EXPORT int CALL InitiateGFX (GFX_INFO Gfx_Info)
{
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice gpu;
    VkQueue queue;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;
    CoreVkVideo_Init(&instance, &device, &gpu, &queue, &get_instance_proc_addr);
    gfx_info = Gfx_Info;
    return parallel_create_device(instance, gpu, device, queue, 0, get_instance_proc_addr);
}

EXPORT void CALL MoveScreen (int xpos, int ypos)
{
}

EXPORT void CALL ProcessDList(void)
{
}

EXPORT void CALL ProcessRDPList(void)
{
    RDP::process_commands(gfx_info);
}

EXPORT int CALL RomOpen (void)
{
    return RDP::init(gfx_info);
}

EXPORT void CALL RomClosed (void)
{
    RDP::deinit();
}

EXPORT void CALL ShowCFB (void)
{
    RDP::complete_frame(gfx_info);
}

EXPORT void CALL UpdateScreen (void)
{
    RDP::complete_frame(gfx_info);
}

EXPORT void CALL ViStatusChanged (void)
{
}

EXPORT void CALL ViWidthChanged (void)
{
}

EXPORT void CALL ChangeWindow(void)
{
}

EXPORT void CALL ReadScreen2(void *dest, int *width, int *height, int front)
{
}

EXPORT void CALL SetRenderingCallback(void (*callback)(int))
{
    render_callback = callback;
}

EXPORT void CALL ResizeVideoOutput(int width, int height)
{
}

EXPORT void CALL FBWrite(unsigned int addr, unsigned int size)
{
}

EXPORT void CALL FBRead(unsigned int addr)
{
}

EXPORT void CALL FBGetFrameBufferInfo(void *pinfo)
{
}
