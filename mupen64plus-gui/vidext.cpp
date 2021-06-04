#include "vidext.h"
#include "common.h"
#include "workerthread.h"
#include "mainwindow.h"
#include "interface/core_commands.h"
#include <stdio.h>
#include <QDesktopWidget>
#include <QScreen>

static int init;
static int needs_toggle;
static int set_volume;

PFN_vkVoidFunction qtvkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    return w->getVulkanWindow()->vulkanInstance()->getInstanceProcAddr(pName);
}

uint32_t qtVidExtFuncVkGetSyncIndex()
{
    return 0;
}

uint32_t qtVidExtFuncVkGetSyncIndexMask()
{
    return (1 << 32) - 1;
}

m64p_error qtVidExtFuncVkInit(VkInstance* instance, VkSurfaceKHR* surface, VkPhysicalDevice* gpu, PFN_vkGetInstanceProcAddr* func)
{
    w->getWorkerThread()->createVulkanWindow();
    *instance = w->getVulkanWindow()->vulkanInstance()->vkInstance();
    *surface = QVulkanInstance::surfaceForWindow(w->getVulkanWindow());
    *gpu = NULL;
    *func = qtvkGetInstanceProcAddr;
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncInit(void)
{
    init = 0;
    set_volume = 1;

    w->setRenderingThread(QThread::currentThread());
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncQuit(void)
{
    init = 0;
    w->getWorkerThread()->toggleFS(M64VIDEO_WINDOWED);
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncListModes(m64p_2d_size *SizeArray, int *NumSizes)
{
    QList<QScreen *> screens = QGuiApplication::screens();
    QRect size = screens.first()->geometry();
    SizeArray[0].uiWidth = size.width();
    SizeArray[0].uiHeight = size.height();
    *NumSizes = 1;
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncListRates(m64p_2d_size, int*, int*)
{
    return M64ERR_UNSUPPORTED;
}

m64p_error qtVidExtFuncSetMode(int Width, int Height, int, int ScreenMode, int)
{
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncSetModeWithRate(int, int, int, int, int, int)
{
    return M64ERR_UNSUPPORTED;
}

m64p_function qtVidExtFuncGLGetProc(const char* Proc)
{
    if (!init) return NULL;
    return NULL;
}

m64p_error qtVidExtFuncGLSetAttr(m64p_GLattr Attr, int Value)
{
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncGLGetAttr(m64p_GLattr Attr, int *pValue)
{
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncGLSwapBuf(void)
{
    if (set_volume) {
        int value;
        (*CoreDoCommand)(M64CMD_CORE_STATE_QUERY, M64CORE_EMU_STATE, &value);
        if (value == M64EMU_RUNNING) {
            QSettings settings(w->getSettings()->fileName(), QSettings::IniFormat);
            int volume = settings.value("volume").toInt();
            (*CoreDoCommand)(M64CMD_CORE_STATE_SET, M64CORE_AUDIO_VOLUME, &volume);
            set_volume = 0;
        }
    }

    if (needs_toggle) {
        int value;
        (*CoreDoCommand)(M64CMD_CORE_STATE_QUERY, M64CORE_EMU_STATE, &value);
        if (value > M64EMU_STOPPED) {
            w->getWorkerThread()->toggleFS(needs_toggle);
            needs_toggle = 0;
        }
    }

#ifdef SINGLE_THREAD
    QCoreApplication::processEvents();
#endif
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncSetCaption(const char *)
{
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncToggleFS(void)
{
    if (QThread::currentThread() == w->getRenderingThread())
        w->getWorkerThread()->toggleFS(M64VIDEO_NONE);
    else
        w->toggleFS(M64VIDEO_NONE);

    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncResizeWindow(int width, int height)
{
    int response = M64VIDEO_NONE;
    (*CoreDoCommand)(M64CMD_CORE_STATE_QUERY, M64CORE_VIDEO_MODE, &response);
    if (response == M64VIDEO_WINDOWED)
    {
        int size = (width << 16) + height;
        int current_size = 0;
        (*CoreDoCommand)(M64CMD_CORE_STATE_QUERY, M64CORE_VIDEO_SIZE, &current_size);
        if (current_size != size)
            w->getWorkerThread()->resizeMainWindow(width, height);
    }
    return M64ERR_SUCCESS;
}

uint32_t qtVidExtFuncGLGetDefaultFramebuffer(void)
{
    return 0;
}
