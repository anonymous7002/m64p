#ifndef GFX_M64P_H
#define GFX_M64P_H

#include "m64p_plugin.h"
#include "m64p_common.h"

#ifdef _WIN32
#define DLSYM(a, b) GetProcAddress(a, b)
#else
#include <dlfcn.h>
#define DLSYM(a, b) dlsym(a, b)
#endif

#endif