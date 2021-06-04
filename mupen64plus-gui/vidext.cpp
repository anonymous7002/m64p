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
static QSurfaceFormat format;

PFN_vkVoidFunction qtvkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    return w->getVulkanWindow()->vulkanInstance()->getInstanceProcAddr(pName);
}

uint32_t qtVidExtFuncVkGetSyncIndex()
{
    return w->getVulkanWindow()->currentSwapChainImageIndex();
}

uint32_t qtVidExtFuncVkGetSyncIndexMask()
{
    return (1 << w->getVulkanWindow()->swapChainImageCount()) - 1;
}

m64p_error qtVidExtFuncVkInit(VkInstance* instance, VkSurfaceKHR* surface, VkPhysicalDevice* gpu, PFN_vkGetInstanceProcAddr* func)
{
    w->getWorkerThread()->createVulkanWindow();
    while (!w->getVulkanWindow()->isValid()) {}
    *instance = w->getVulkanWindow()->vulkanInstance()->vkInstance();
    *surface = QVulkanInstance::surfaceForWindow(w->getVulkanWindow());
    *gpu = w->getVulkanWindow()->physicalDevice();
    *func = qtvkGetInstanceProcAddr;
    return M64ERR_SUCCESS;
}

m64p_error qtVidExtFuncInit(void)
{
    init = 0;
    set_volume = 1;
    format = QSurfaceFormat::defaultFormat();
    format.setOption(QSurfaceFormat::DeprecatedFunctions, 1);
    format.setDepthBufferSize(24);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setMajorVersion(2);
    format.setMinorVersion(1);
    format.setSwapInterval(0);
    if (w->getGLES())
        format.setRenderableType(QSurfaceFormat::OpenGLES);

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
    if (!init) {
        w->getWorkerThread()->createVulkanWindow();
#ifdef SINGLE_THREAD
        QCoreApplication::processEvents();
#else
        while (!w->getVulkanWindow()->isValid()) {}
#endif
        w->getWorkerThread()->resizeMainWindow(Width, Height);
        init = 1;
        needs_toggle = ScreenMode;
    }
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
    switch (Attr) {
    case M64P_GL_DOUBLEBUFFER:
        if (Value == 1)
            format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        else if (Value == 0)
            format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
        break;
    case M64P_GL_BUFFER_SIZE:
        break;
    case M64P_GL_DEPTH_SIZE:
        format.setDepthBufferSize(Value);
        break;
    case M64P_GL_RED_SIZE:
        format.setRedBufferSize(Value);
        break;
    case M64P_GL_GREEN_SIZE:
        format.setGreenBufferSize(Value);
        break;
    case M64P_GL_BLUE_SIZE:
        format.setBlueBufferSize(Value);
        break;
    case M64P_GL_ALPHA_SIZE:
        format.setAlphaBufferSize(Value);
        break;
    case M64P_GL_SWAP_CONTROL:
        format.setSwapInterval(Value);
        break;
    case M64P_GL_MULTISAMPLEBUFFERS:
        break;
    case M64P_GL_MULTISAMPLESAMPLES:
        format.setSamples(Value);
        break;
    case M64P_GL_CONTEXT_MAJOR_VERSION:
        format.setMajorVersion(Value);
        break;
    case M64P_GL_CONTEXT_MINOR_VERSION:
        format.setMinorVersion(Value);
        break;
    case M64P_GL_CONTEXT_PROFILE_MASK:
        switch (Value) {
        case M64P_GL_CONTEXT_PROFILE_CORE:
            format.setProfile(QSurfaceFormat::CoreProfile);
            break;
        case M64P_GL_CONTEXT_PROFILE_COMPATIBILITY:
            format.setProfile(QSurfaceFormat::CompatibilityProfile);
            break;
        case M64P_GL_CONTEXT_PROFILE_ES:
            format.setRenderableType(QSurfaceFormat::OpenGLES);
            break;
        }

        break;
    }

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
