cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
project(parallel-rdp LANGUAGES CXX C)

set(NAME_PLUGIN_M64P "mupen64plus-video-parallel")
include_directories(../mupen64plus-core/src/api)
add_definitions(-DM64P_PLUGIN_API -DGRANITE_VULKAN_MT)

if (WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
endif()

FILE(GLOB RDPSources standalone/parallel-rdp/*.cpp)

add_library(${NAME_PLUGIN_M64P} SHARED
    ${RDPSources}
    standalone/vulkan/buffer.cpp
    standalone/vulkan/buffer_pool.cpp
    standalone/vulkan/command_buffer.cpp
    standalone/vulkan/command_pool.cpp
    standalone/vulkan/context.cpp
    standalone/vulkan/cookie.cpp
    standalone/vulkan/descriptor_set.cpp
    standalone/vulkan/device.cpp
    standalone/vulkan/event_manager.cpp
    standalone/vulkan/fence.cpp
    standalone/vulkan/fence_manager.cpp
    standalone/vulkan/image.cpp
    standalone/vulkan/memory_allocator.cpp
    standalone/vulkan/pipeline_event.cpp
    standalone/vulkan/query_pool.cpp
    standalone/vulkan/render_pass.cpp
    standalone/vulkan/sampler.cpp
    standalone/vulkan/semaphore.cpp
    standalone/vulkan/semaphore_manager.cpp
    standalone/vulkan/shader.cpp
    standalone/vulkan/texture_format.cpp
    standalone/util/logging.cpp
    standalone/util/thread_id.cpp
    standalone/util/aligned_alloc.cpp
    standalone/util/timer.cpp
    standalone/util/timeline_trace_file.cpp
    standalone/util/thread_name.cpp
    standalone/volk/volk.c
    gfx_m64p.cpp
    rdp.cpp)

target_include_directories(${NAME_PLUGIN_M64P} PUBLIC
    standalone/parallel-rdp
    standalone/volk
    standalone/vulkan
    standalone/vulkan-headers/include
    standalone/util)

set_target_properties(${NAME_PLUGIN_M64P} PROPERTIES PREFIX "")
