#include "vulkan_headers.hpp"
#include "rdp_device.hpp"
#include "context.hpp"
#include "device.hpp"
#include "rdp.h"
#include <stdint.h>
#include <memory>

#define DP_STATUS_XBUS_DMA    0x01
#define DP_INTERRUPT          0x20

using namespace Vulkan;
using namespace std;

namespace RDP
{
const struct retro_hw_render_interface_vulkan *vulkan;
static int cmd_cur;
static int cmd_ptr;
static uint32_t cmd_data[0x00040000 >> 2];

static unique_ptr<CommandProcessor> frontend;
static unique_ptr<Device> device;
static unique_ptr<Context> context;
static QueryPoolHandle begin_ts, end_ts;

static vector<retro_vulkan_image> retro_images;
static vector<ImageHandle> retro_image_handles;
unsigned width, height;
unsigned overscan;
unsigned upscaling = 1;
unsigned downscaling_steps = 0;
bool native_texture_lod = false;
bool native_tex_rect = true;
bool synchronous = true, divot_filter = true, gamma_dither = true;
bool vi_aa = true, vi_scale = true, dither_filter = true;
bool interlacing = true, super_sampled_read_back = false, super_sampled_dither = true;

static const unsigned cmd_len_lut[64] = {
	1, 1, 1, 1, 1, 1, 1, 1, 4, 6, 12, 14, 12, 14, 20, 22,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,
	1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,
};

void process_commands(GFX_INFO gfx_info)
{
	const uint32_t DP_CURRENT = *gfx_info.DPC_CURRENT_REG & 0x00FFFFF8;
	const uint32_t DP_END = *gfx_info.DPC_END_REG & 0x00FFFFF8;
	// This works in parallel-n64, but not this repo for some reason.
	// Angrylion does not clear this bit here.
	//*GET_GFX_INFO(DPC_STATUS_REG) &= ~DP_STATUS_FREEZE;

	int length = DP_END - DP_CURRENT;
	if (length <= 0)
		return;

	length = unsigned(length) >> 3;
	if ((cmd_ptr + length) & ~(0x0003FFFF >> 3))
		return;

	uint32_t offset = DP_CURRENT;
	if (*gfx_info.DPC_STATUS_REG & DP_STATUS_XBUS_DMA)
	{
		do
		{
			offset &= 0xFF8;
			cmd_data[2 * cmd_ptr + 0] = *reinterpret_cast<const uint32_t *>(gfx_info.DMEM + offset);
			cmd_data[2 * cmd_ptr + 1] = *reinterpret_cast<const uint32_t *>(gfx_info.DMEM + offset + 4);
			offset += sizeof(uint64_t);
			cmd_ptr++;
		} while (--length > 0);
	}
	else
	{
		if (DP_END > 0x7ffffff || DP_CURRENT > 0x7ffffff)
		{
			return;
		}
		else
		{
			do
			{
				offset &= 0xFFFFF8;
				cmd_data[2 * cmd_ptr + 0] = *reinterpret_cast<const uint32_t *>(gfx_info.RDRAM + offset);
				cmd_data[2 * cmd_ptr + 1] = *reinterpret_cast<const uint32_t *>(gfx_info.RDRAM + offset + 4);
				offset += sizeof(uint64_t);
				cmd_ptr++;
			} while (--length > 0);
		}
	}

	while (cmd_cur - cmd_ptr < 0)
	{
		uint32_t w1 = cmd_data[2 * cmd_cur];
		uint32_t command = (w1 >> 24) & 63;
		int cmd_length = cmd_len_lut[command];

		if (cmd_ptr - cmd_cur - cmd_length < 0)
		{
			*gfx_info.DPC_START_REG = *gfx_info.DPC_CURRENT_REG = *gfx_info.DPC_END_REG;
			return;
		}

		if (command >= 8 && frontend)
			frontend->enqueue_command(cmd_length * 2, &cmd_data[2 * cmd_cur]);

		if (RDP::Op(command) == RDP::Op::SyncFull)
		{
			// For synchronous RDP:
			if (synchronous && frontend)
				frontend->wait_for_timeline(frontend->signal_timeline());
			*gfx_info.MI_INTR_REG |= DP_INTERRUPT;
			gfx_info.CheckInterrupts();
		}

		cmd_cur += cmd_length;
	}

	cmd_ptr = 0;
	cmd_cur = 0;
	*gfx_info.DPC_START_REG = *gfx_info.DPC_CURRENT_REG = *gfx_info.DPC_END_REG;
}

bool init(GFX_INFO gfx_info)
{
	if (!context || !vulkan)
		return false;

	unsigned mask = vulkan->get_sync_index_mask(vulkan->handle);
	unsigned num_frames = 0;
	unsigned num_sync_frames = 0;
	for (unsigned i = 0; i < 32; i++)
	{
		if (mask & (1u << i))
		{
			num_frames = i + 1;
			num_sync_frames++;
		}
	}

	retro_images.resize(num_frames);
	retro_image_handles.resize(num_frames);

	device.reset(new Device);
	device->set_context(*context);
	device->init_frame_contexts(num_sync_frames);
	device->set_queue_lock(
			[]() { vulkan->lock_queue(vulkan->handle); },
			[]() { vulkan->unlock_queue(vulkan->handle); });

	uintptr_t aligned_rdram = reinterpret_cast<uintptr_t>(gfx_info.RDRAM);
	uintptr_t offset = 0;

	if (device->get_device_features().supports_external_memory_host)
	{
		size_t align = device->get_device_features().host_memory_properties.minImportedHostPointerAlignment;
		offset = aligned_rdram & (align - 1);

		// Mupen64Plus allocates RDRAM on the heap, so this can be guaranteed to be 0.
		if (offset)
		{
			return false;
		}
		aligned_rdram -= offset;
	}

	unsigned rdram_size = 8 * 1024 * 1024;
	if (gfx_info.version >= 2 && gfx_info.RDRAM_SIZE)
		rdram_size = *gfx_info.RDRAM_SIZE;

	if (rdram_size == 0)
	{
		return false;
	}

	CommandProcessorFlags flags = 0;
	switch (upscaling)
	{
		case 2:
			flags |= COMMAND_PROCESSOR_FLAG_UPSCALING_2X_BIT;
			break;

		case 4:
			flags |= COMMAND_PROCESSOR_FLAG_UPSCALING_4X_BIT;
			break;

		case 8:
			flags |= COMMAND_PROCESSOR_FLAG_UPSCALING_8X_BIT;
			break;

		default:
			break;
	}

	if (upscaling > 1 && super_sampled_read_back)
		flags |= COMMAND_PROCESSOR_FLAG_SUPER_SAMPLED_READ_BACK_BIT;
	if (super_sampled_dither)
		flags |= COMMAND_PROCESSOR_FLAG_SUPER_SAMPLED_DITHER_BIT;

	frontend.reset(new CommandProcessor(*device, reinterpret_cast<void *>(aligned_rdram),
				offset, rdram_size, rdram_size / 2, flags));

	if (!frontend->device_is_supported())
	{
		frontend.reset();
		return false;
	}

	RDP::Quirks quirks;
	quirks.set_native_texture_lod(native_texture_lod);
	quirks.set_native_resolution_tex_rect(native_tex_rect);
	frontend->set_quirks(quirks);

	width = 0;
	height = 0;
	return true;
}

void deinit()
{
	begin_ts.reset();
	end_ts.reset();
	retro_image_handles.clear();
	retro_images.clear();
	frontend.reset();
	device.reset();
	context.reset();
}

static void complete_frame_error()
{
	static const char error_tex[] =
		"ooooooooooooooooooooooooo"
		"ooXXXXXoooXXXXXoooXXXXXoo"
		"ooXXooooooXoooXoooXoooXoo"
		"ooXXXXXoooXXXXXoooXXXXXoo"
		"ooXXXXXoooXoXoooooXoXoooo"
		"ooXXooooooXooXooooXooXooo"
		"ooXXXXXoooXoooXoooXoooXoo"
		"ooooooooooooooooooooooooo";

	auto info = Vulkan::ImageCreateInfo::immutable_2d_image(50, 16, VK_FORMAT_R8G8B8A8_UNORM, false);
	info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	info.misc = IMAGE_MISC_MUTABLE_SRGB_BIT;

	Vulkan::ImageInitialData data = {};

	uint32_t tex_data[16][50];
	for (unsigned y = 0; y < 16; y++)
		for (unsigned x = 0; x < 50; x++)
			tex_data[y][x] = error_tex[25 * (y >> 1) + (x >> 1)] != 'o' ? 0xffffffffu : 0u;
	data.data = tex_data;
	auto image = device->create_image(info, &data);

	unsigned index = vulkan->get_sync_index(vulkan->handle);
	assert(index < retro_images.size());

	retro_images[index].image_view = image->get_view().get_view();
	retro_images[index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	retro_images[index].create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	retro_images[index].create_info.image = image->get_image();
	retro_images[index].create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	retro_images[index].create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	retro_images[index].create_info.subresourceRange.baseMipLevel = 0;
	retro_images[index].create_info.subresourceRange.baseArrayLayer = 0;
	retro_images[index].create_info.subresourceRange.levelCount = 1;
	retro_images[index].create_info.subresourceRange.layerCount = 1;
	retro_images[index].create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	retro_images[index].create_info.components.r = VK_COMPONENT_SWIZZLE_R;
	retro_images[index].create_info.components.g = VK_COMPONENT_SWIZZLE_G;
	retro_images[index].create_info.components.b = VK_COMPONENT_SWIZZLE_B;
	retro_images[index].create_info.components.a = VK_COMPONENT_SWIZZLE_A;

	vulkan->set_image(vulkan->handle, &retro_images[index], 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
	width = image->get_width();
	height = image->get_height();
	retro_image_handles[index] = image;

	device->flush_frame();
}

void complete_frame(GFX_INFO gfx_info)
{
	if (!frontend)
	{
		complete_frame_error();
		device->next_frame_context();
		return;
	}

	frontend->set_vi_register(VIRegister::Control, *gfx_info.VI_STATUS_REG);
	frontend->set_vi_register(VIRegister::Origin, *gfx_info.VI_ORIGIN_REG);
	frontend->set_vi_register(VIRegister::Width, *gfx_info.VI_WIDTH_REG);
	frontend->set_vi_register(VIRegister::Intr, *gfx_info.VI_INTR_REG);
	frontend->set_vi_register(VIRegister::VCurrentLine, *gfx_info.VI_V_CURRENT_LINE_REG);
	frontend->set_vi_register(VIRegister::Timing, *gfx_info.VI_V_BURST_REG);
	frontend->set_vi_register(VIRegister::VSync, *gfx_info.VI_V_SYNC_REG);
	frontend->set_vi_register(VIRegister::HSync, *gfx_info.VI_H_SYNC_REG);
	frontend->set_vi_register(VIRegister::Leap, *gfx_info.VI_LEAP_REG);
	frontend->set_vi_register(VIRegister::HStart, *gfx_info.VI_H_START_REG);
	frontend->set_vi_register(VIRegister::VStart, *gfx_info.VI_V_START_REG);
	frontend->set_vi_register(VIRegister::VBurst, *gfx_info.VI_V_BURST_REG);
	frontend->set_vi_register(VIRegister::XScale, *gfx_info.VI_X_SCALE_REG);
	frontend->set_vi_register(VIRegister::YScale, *gfx_info.VI_Y_SCALE_REG);

	ScanoutOptions opts;
	opts.persist_frame_on_invalid_input = true;
	opts.vi.aa = vi_aa;
	opts.vi.scale = vi_scale;
	opts.vi.dither_filter = dither_filter;
	opts.vi.divot_filter = divot_filter;
	opts.vi.gamma_dither = gamma_dither;
	opts.blend_previous_frame = interlacing;
	opts.upscale_deinterlacing = !interlacing;
	opts.downscale_steps = downscaling_steps;
	opts.crop_overscan_pixels = overscan;
	auto image = frontend->scanout(opts);
	unsigned index = vulkan->get_sync_index(vulkan->handle);

	if (!image)
	{
		auto info = Vulkan::ImageCreateInfo::immutable_2d_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		info.misc = IMAGE_MISC_MUTABLE_SRGB_BIT;
		info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		image = device->create_image(info);

		auto cmd = device->request_command_buffer();
		cmd->image_barrier(*image,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
		cmd->clear_image(*image, {});
		cmd->image_barrier(*image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
		device->submit(cmd);
	}

	assert(index < retro_images.size());

	retro_images[index].image_view = image->get_view().get_view();
	retro_images[index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	retro_images[index].create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	retro_images[index].create_info.image = image->get_image();
	retro_images[index].create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	retro_images[index].create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	retro_images[index].create_info.subresourceRange.baseMipLevel = 0;
	retro_images[index].create_info.subresourceRange.baseArrayLayer = 0;
	retro_images[index].create_info.subresourceRange.levelCount = 1;
	retro_images[index].create_info.subresourceRange.layerCount = 1;
	retro_images[index].create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	retro_images[index].create_info.components.r = VK_COMPONENT_SWIZZLE_R;
	retro_images[index].create_info.components.g = VK_COMPONENT_SWIZZLE_G;
	retro_images[index].create_info.components.b = VK_COMPONENT_SWIZZLE_B;
	retro_images[index].create_info.components.a = VK_COMPONENT_SWIZZLE_A;

	vulkan->set_image(vulkan->handle, &retro_images[index], 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
	width = image->get_width();
	height = image->get_height();
	retro_image_handles[index] = image;

	end_ts = device->write_calibrated_timestamp();
	device->register_time_interval("Emulation", begin_ts, end_ts, "frame");
	begin_ts.reset();
	end_ts.reset();

	RDP::Quirks quirks;
	quirks.set_native_texture_lod(native_texture_lod);
	quirks.set_native_resolution_tex_rect(native_tex_rect);
	frontend->set_quirks(quirks);

	frontend->begin_frame_context();
}
}

static const char *device_extensions[] = {
    "VK_KHR_swapchain",
};

bool parallel_create_device(VkInstance instance, VkPhysicalDevice gpu,
                            VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr)
{
	if (!Vulkan::Context::init_loader(get_instance_proc_addr))
		return false;

	::RDP::context.reset(new Vulkan::Context);

	::Vulkan::Context::SystemHandles handles;

	::RDP::context->set_system_handles(handles);

    const VkPhysicalDeviceFeatures features = { 0 };
	if (!::RDP::context->init_device_from_instance(
				instance, gpu, surface, device_extensions, 1,
				NULL, 0, &features, Vulkan::CONTEXT_CREATION_DISABLE_BINDLESS_BIT))
	{
		::RDP::context.reset();
		return false;
	}

	// Frontend owns the device.
	::RDP::context->release_device();
	return true;
}
